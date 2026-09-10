# Wagnostic Zero-Dependency Terminal Runner (`wagnostic.js`)

Runner universal em arquivo único para jogos e ROMs do **Wagnostic** (`.wasm`), compatível com **Node.js** e **txiki.js (`tjs`)**, sem nenhuma dependência externa (`npm`).

## Características

- 📄 **Single-File Host**: Todo o runtime, decodificador de structs da ABI (`std:*`), encoder LZW de GIF e renderizador ANSI TrueColor estão contidos em `wagnostic.js`.
- 🪶 **Zero Dependências**: Não requer `npm install` nem bibliotecas nativas de janela. Funciona diretamente com a engine WebAssembly embutida do Node.js ou Txiki.
- 📺 **Renderização ANSI TrueColor**: Renderiza o framebuffer 32-bit RGBA diretamente no terminal usando blocos Unicode (`▀`) de alta densidade (2 pixels verticais por caractere) com sequências de escape ANSI de 24 bits.
- 🎬 **Exportação de GIF Headless**: Permite gravar animações GIF diretamente via linha de comando (`-g output.gif -n 60`).
- 🎮 **Controles Interativos no Terminal**: Mapeamento de teclado raw mode no terminal (WASD/Setas, Z/X/C/V, Q para sair).
- 🔄 **Compatibilidade Dual**: Detecta e roda perfeitamente em Node.js (`node`) ou Txiki (`tjs`).

## Como Executar

### Com Node.js:

```bash
# Executar interativamente no terminal:
node runners/node/wagnostic.js roms/display_test.wasm

# Ou diretamente como executável:
./runners/node/wagnostic.js roms/display_test.wasm

# Gravar GIF em modo headless:
node runners/node/wagnostic.js -g demo.gif -n 60 roms/display_test.wasm
```

### Com Txiki.js (`tjs`):

```bash
tjs run runners/node/wagnostic.js roms/display_test.wasm
```

### Opções de Linha de Comando

- `-g <file.gif>`: Salva animação em arquivo GIF.
- `-n <frames>`: Limita a execução a N quadros (útil para gravações headless ou testes).
- `--headless`: Executa sem renderizar no terminal (ideal para benchmarks ou CI).
- `-h`, `--help`: Mostra mensagem de ajuda.

