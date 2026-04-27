int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s game.wasm\n", argv[0]); return 1; }

    IM3Environment env     = m3_NewEnvironment();
    runtime = m3_NewRuntime(env, 8 * 1024 * 1024, NULL); 
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); size_t wsize = ftell(f); rewind(f);
    uint8_t* wasm = malloc(wsize);
    fread(wasm, 1, wsize, f); fclose(f);

    IM3Module module;
    M3Result result = m3_ParseModule(env, &module, wasm, wsize);
    if (result) { fprintf(stderr, "m3_ParseModule error: %s\n", result); return 1; }
    
    result = m3_LoadModule(runtime, module);
    if (result) { fprintf(stderr, "m3_LoadModule error: %s\n", result); return 1; }

    uint32_t current_mem_size = 0;
    uint8_t* initial_mem = m3_GetMemory(runtime, &current_mem_size, 0);
    if (initial_mem && current_mem_size >= 1024) {
        memset(initial_mem, 0, 1024);
    }

    m3_LinkRawFunction(module, "env", "get_ticks", "i()", host_get_ticks);

    IM3Function fn_init = NULL;
    m3_FindFunction(&fn_init, runtime, "winit");
    if (fn_init) {
        result = m3_CallV(fn_init);
        if (result) { fprintf(stderr, "m3_CallV (winit) error: %s\n", result); return 1; }
    }

    // Apply Fallback Defaults if the ROM didn't set them
    uint8_t* mem_ptr = m3_GetMemory(runtime, NULL, 0);
    char last_title[128] = {0};
    if (mem_ptr) {
        SystemConfig* sys = (SystemConfig*)(mem_ptr + sys_wasm_offset);
        if (sys->width == 0) sys->width = 320;
        if (sys->height == 0) sys->height = 240;
        if (sys->bpp == 0) sys->bpp = 8;
        if (sys->scale == 0) sys->scale = 1;
        if (sys->signal_count == 0) sys->signal_count = 4;
        if (sys->message[0] == '\0') {
            const char* def_title = "Wagnostic";
            for(int i=0; i<127 && def_title[i]; i++) sys->message[i] = def_title[i];
        }
        strncpy(last_title, sys->message, 127);
    }

    IM3Function fn_frame;
    result = m3_FindFunction(&fn_frame, runtime, "wupdate");
    if (result) { fprintf(stderr, "m3_FindFunction (wupdate) error: %s\n", result); return 1; }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    init_sdl_from_header();
    
    // init_sdl_from_header creates the window if it's NULL
    if (!window) { fprintf(stderr, "Failed to create window\n"); return 1; }
    
    gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
#if defined(_WIN32) && defined(_MSC_VER)
    glewExperimental = GL_TRUE;
    GLenum glew_err = glewInit();
    if (glew_err != GLEW_OK) {
        fprintf(stderr, "GLEW init error: %s\n", glewGetErrorString(glew_err));
        return 1;
    }
#endif

    GLuint prog = build_program();
    if (!prog) { fprintf(stderr, "Failed to build shader program\n"); return 1; }
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "u_fb"), 0);

    float quad[] = {
        -1.f, -1.f,  0.f, 1.f,
         1.f, -1.f,  1.f, 1.f,
        -1.f,  1.f,  0.f, 0.f,
         1.f,  1.f,  1.f, 0.f,
    };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);

    glGenTextures(1, &fb_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    int running = 1;
    SDL_Event event;
    uint32_t last_time = SDL_GetTicks();

    while (running) {
        uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
        if (!mem) { SDL_Delay(10); continue; }
        SystemConfig* sys = (SystemConfig*)(mem + sys_wasm_offset);
        uint8_t* signals = mem + 512;

        int should_redraw = 0;
        for (uint32_t i = 0; i < sys->signal_count; i++) {
            uint8_t sig = signals[i];
            if (sig == 0) continue;
            if (sig == 1) { should_redraw = 1; }
            else if (sig == 2) { running = 0; }
            else if (sig == 3 || sig == 4 || sig == 5) { init_sdl_from_header(); }
            else if (sig == 6) { printf("INFO: %s\n", sys->message); }
            else if (sig == 7) { printf("WARN: %s\n", sys->message); }
            else if (sig == 8) { fprintf(stderr, "ERROR: %s\n", sys->message); }
            signals[i] = 0;
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                int down = (event.type == SDL_KEYDOWN);
                if (event.key.keysym.scancode < 256) sys->keys[event.key.keysym.scancode] = down ? 1 : 0;
                uint32_t bit = 0;
                switch (event.key.keysym.sym) {
                    case SDLK_UP:     bit = BTN_UP; break;
                    case SDLK_DOWN:   bit = BTN_DOWN; break;
                    case SDLK_LEFT:   bit = BTN_LEFT; break;
                    case SDLK_RIGHT:  bit = BTN_RIGHT; break;
                    case SDLK_z:      bit = BTN_A; break;
                    case SDLK_x:      bit = BTN_B; break;
                    case SDLK_RETURN: bit = BTN_START; break;
                    case SDLK_ESCAPE: bit = BTN_SELECT; break;
                }
                if (bit) { if (down) sys->gamepad_buttons |= bit; else sys->gamepad_buttons &= ~bit; }
            }
            if (event.type == SDL_MOUSEMOTION) {
                int ww, wh; SDL_GetWindowSize(window, &ww, &wh);
                if (ww > 0 && wh > 0) {
                    sys->mouse_x = (event.motion.x * W) / ww;
                    sys->mouse_y = (event.motion.y * H) / wh;
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                int down = (event.type == SDL_MOUSEBUTTONDOWN);
                uint32_t bit = (1 << (event.button.button - 1));
                if (down) sys->mouse_buttons |= bit; else sys->mouse_buttons &= ~bit;
            }
            if (event.type == SDL_MOUSEWHEEL) sys->mouse_wheel += event.wheel.y;
        }

        if (fn_frame) m3_CallV(fn_frame);

        mem = m3_GetMemory(runtime, NULL, 0);
        if (mem) {
            sys = (SystemConfig*)(mem + sys_wasm_offset);
            uint8_t* fb = mem + 512 + sys->signal_count;
            if (fb_tex && W > 0 && H > 0 && should_redraw) {
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, fb_tex);
                if (sys->bpp == 32) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, fb);
                else if (sys->bpp == 8 || sys->bpp == 16) {
                    static uint32_t* temp_fb32 = NULL;
                    temp_fb32 = realloc(temp_fb32, W * H * 4);
                    if (sys->bpp == 8) {
                        for(int i=0; i<W*H; i++) {
                            uint8_t c = fb[i];
                            uint8_t r = ((c >> 5) & 0x07) * 255 / 7, g = ((c >> 2) & 0x07) * 255 / 7, b = (c & 0x03) * 255 / 3;
                            temp_fb32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
                        }
                    } else {
                        uint16_t* fb16 = (uint16_t*)fb;
                        for(int i=0; i<W*H; i++) {
                            uint16_t c = fb16[i];
                            uint8_t r = ((c >> 11) & 0x1F) * 255 / 31, g = ((c >> 5) & 0x3F) * 255 / 63, b = (c & 0x1F) * 255 / 31;
                            temp_fb32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
                        }
                    }
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, temp_fb32);
                }
            }
            if (should_redraw) {
                glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                SDL_GL_SwapWindow(window);
            }
        }

        uint32_t now = SDL_GetTicks();
        uint32_t elapsed = now - last_time;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
        last_time = SDL_GetTicks();
    }
    return 0;
}
