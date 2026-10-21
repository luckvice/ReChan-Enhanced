<p align="right">
  <a href="README.md">English</a> |
  <b>Português (Brasil)</b> |
  <a href="README.es.md">Español</a>
</p>

<table align="center">
<tr>
<td valign="middle">

# ReChan — Edição Aprimorada & Otimizada
**Por Luckvice**

**ReChan** é uma reimplementação aberta via engenharia reversa do clássico do PlayStation **Jackie Chan Stuntmaster** (SLUS-00684).

Este fork mantido por **Luckvice** tem como objetivo proporcionar uma experiência de jogo definitiva, altamente otimizada e aprimorada para hardwares modernos e retrô, trazendo grandes reformulações de desempenho, portes nativos dedicados para consoles, refinamentos visuais e inúmeras melhorias de qualidade de vida.

> [!NOTE]
> **Agradecimentos Especiais & Reconhecimento**: Nossos sinceros agradecimentos ao criador original ([SilverwireGames](https://github.com/SilverwireGames/ReChan)) e a todos os contribuidores da base original de engenharia reversa. O propósito deste fork é levar a experiência de *Jackie Chan Stuntmaster* ao seu mais alto nível de fidelidade e performance.

> [!IMPORTANT]
> **Status Oficial do Projeto**: Estamos oficialmente na **Beta 5**! Os esforços atuais estão focados em consolidar a estabilidade, o desempenho e o polimento geral em todas as plataformas e na engine base, preparando o terreno para a chegada iminente da **RC1 (Release Candidate 1)**.

### Para utilizar este projeto:
* Você **deve possuir uma cópia legal** de *Jackie Chan Stuntmaster* (SLUS-00684).
* Você deve fornecer sua própria imagem de disco do jogo.
* Todos os direitos do jogo original, recursos e propriedade intelectual pertencem aos seus respectivos proprietários.

<br>
<img src="https://img.shields.io/badge/marco-Beta%205-orange?style=flat-square" />
<img src="https://img.shields.io/badge/pr%C3%B3ximo-RC1%20em%20breve-yellow?style=flat-square" />
<img src="https://img.shields.io/badge/otimizado%20por-Luckvice-blue?style=flat-square" />
<img src="https://img.shields.io/badge/licen%C3%A7a-MIT-green?style=flat-square" />
</td>

<td align="center" valign="middle" width="256">
<img src="res/rechanlogo512.png" width="256">
</td>
</tr>
</table>

## Sobre o Projeto
* Versão alvo: **SLUS-00684** (NTSC-U)
* Início do desenvolvimento: **Outubro de 2025**
* Status atual: **Beta 5** (RC1 em breve)

O projeto foca em:
* Reconstrução da lógica e dos sistemas originais do jogo
* Reconstrução das estruturas de dados e reprodução autêntica do comportamento em tempo de execução
* Disponibilização de portes nativos ultra-otimizados para plataformas clássicas e modernas (PSP, macOS Metal, PS2 WIP)
* Elevação do desempenho, fidelidade visual e qualidade de áudio além dos limites do hardware original

## Aprimoramentos
Além da reconstrução, o projeto introduz diversas melhorias em relação à versão original de PlayStation:
- Suporte a teclado e mouse  
- Suporte a gamepad com vibração, incluindo **barra de luz do DualSense/DualShock 4** e intensidade de vibração configurável  
- Estilos de botões de controle selecionáveis (Xbox, Nintendo Switch e **PlayStation/DualSense**)  
- Suporte a proporção widescreen HOR+  
- Renderização em resoluções mais altas  
- Sombras dinâmicas e opções de anti-aliasing  
- Configurações e aprimoramentos específicos para PC  
- Localização em **Português do Brasil** (além de inglês, alemão, francês, italiano e espanhol)  
- Suporte a legendas em vídeos (`.srt`) e atalhos de salvar/carregar no jogo  
- Melhorias gerais de qualidade de vida  

## Capturas de Tela

### Edição Desktop
<p align="center">
  <img src="res/screenshots/title.png" width="30%">
  <img src="res/screenshots/hub.png" width="30%">
  <img src="res/screenshots/level.png" width="30%">
</p>

### PSP — RE:Chan HD Edition (Nativo 480×272)
<p align="center">
  <img src="res/screenshots_psp/xmb.png" width="24%" alt="Interface XMB do PSP e Artwork Oficial">
  <img src="res/screenshots_psp/title.png" width="24%" alt="Tela de Título Nativa no PSP">
  <img src="res/screenshots_psp/hub.png" width="24%" alt="Hub com HUD Moderno HD e Botões PlayStation">
  <img src="res/screenshots_psp/gameplay.png" width="24%" alt="Gameplay a 60 FPS em Chinatown">
</p>

## Como Começar

Os dados do jogo **não estão inclusos**. Coloque uma imagem NTSC-U obtida legalmente de sua própria cópia (`.bin` ou `.iso`) em uma pasta `discimage` ao lado do executável:

```text
rechan/
|-- rechan.exe       # Windows (use "rechan" no Linux e macOS)
`-- discimage/
    `-- game.bin     # ou game.iso, o nome do arquivo não importa
```

Mantenha apenas uma imagem de disco em `discimage`. Na primeira inicialização, o ReChan verifica a versão do disco e utiliza seu extrator integrado para preparar os arquivos necessários.
Nenhum utilitário de extração externo é necessário.
Assim que os arquivos forem extraídos, a pasta da imagem do disco pode ser removida.

O ReChan suporta atualmente Windows e Linux em x86 de 64 bits (`x86_64`) e macOS no Apple Silicon (`arm64`). No macOS, o renderizador utiliza um backend nativo em Metal. Para compilar você mesmo, consulte o [guia de compilação](docs/visionmaster/business/sources/BUILDING.md). Um porte nativo para PSP também está disponível via PSPSDK. Veja as seções de plataformas abaixo para obter a lista completa de recursos de cada edição.

## Edições por Plataforma

### PSP — *RE:Chan HD Edition* (Beta 5 — RC1 em breve)

<p align="center">
  <img src="res/screenshots_psp/xmb.png" width="48%" alt="Interface XMB do PSP e Artwork Oficial">
  <img src="res/screenshots_psp/title.png" width="48%" alt="Tela de Título Nativa no PSP">
</p>
<p align="center">
  <img src="res/screenshots_psp/hub.png" width="48%" alt="Hub com HUD Moderno HD e Botões PlayStation">
  <img src="res/screenshots_psp/gameplay.png" width="48%" alt="Gameplay a 60 FPS em Chinatown">
</p>

Uma build totalmente nativa para PSP (sem emulação) compilada com o PSPSDK, voltada para a tela de 480×272. Ela é distribuída como uma edição própria com recursos adicionais sobre o jogo base:

- **60 FPS no PSP-2000/3000** (medidos 58,5–60 FPS em todas as fases), com modo opcional **Cinema** travado a 30 FPS estáveis
- Renderização com buffer triplo e chamadas indexadas nativas (memória de lista do GE e índices persistentes)
- **HUD Moderno** (o HUD HD de PC adaptado para 480×272) ou estilo **Clássico**, selecionável na primeira inicialização
- Menus nativos adaptados à interface XMB com **ícones de botões do PlayStation**
- **Três opções adicionais de câmera mais próxima**, além da câmera original
- **Filmes FMV com legendas em seis idiomas** (Português do Brasil, Inglês, Espanhol, Francês, Alemão e Italiano)
- Telas de logo na inicialização, **demo no modo de atração** na tela de título e **rolagem de créditos dos contribuidores**
- **Extração da imagem de disco direto no console**: o PSP extrai os recursos diretamente da imagem do disco do usuário, com escolha de idioma e HUD no primeiro boot — sem necessidade de computador
- Menu de configurações específico para PSP, além de modos de build para depuração/QA, telemetria, teste de estresse (soak) e PRX via PSPLink para desenvolvimento
- Utiliza a partição estendida de memória de usuário (MEMSIZE) no PSP-2000/3000/Go, com otimizações especializadas de heap de baixa memória (<24 MB de partição de usuário) para total compatibilidade com o **PSP-1000 (Fat)**

Requisitos: Qualquer modelo de PSP (PSP-1000 Fat, PSP-2000 Slim, PSP-3000 Brite ou PSP Go) com custom firmware ou HEN, imagem de disco NTSC-U (SLUS-00684) e cerca de ~1.3 GB livres durante a instalação (~600 MB após a conclusão). Pacote de instalação e notas completas: `dist/psp-release/` (Beta 5).

### macOS — Edição Metal

Uma build nativa para Apple Silicon (`arm64`) que renderiza por meio de um **backend Metal dedicado** (sem camada de tradução OpenGL), com todo o conjunto de recursos da versão desktop:

- Renderizador **Metal** nativo desenvolvido sobre a `pddi` (shaders compilados em tempo de execução, MSAA, texturas dinâmicas, overlays ImGui)
- Todos os [Aprimoramentos](#aprimoramentos): widescreen HOR+, renderização em alta resolução, sombras dinâmicas, suporte a DualSense/DualShock com barra de luz, teclado e mouse, todas as localizações, legendas `.srt` para vídeos
- Mesmo fluxo de extração de imagem de disco do Windows/Linux (pasta `discimage/`)
- Harness determinístico `--autotest` com captura de telas via leitura direta do Metal, usado para testes de regressão do renderizador

Para compilar, consulte o [guia de compilação](docs/visionmaster/business/sources/BUILDING.md) (`premake5 gmake2` + `make config=release`).

### PlayStation 2 — Edição Nativa GS (Trabalho em Progresso 🚧)

Um porte totalmente nativo para PlayStation 2 rodando diretamente no processador **Emotion Engine (EE)** do console e renderizando no hardware **Graphics Synthesizer (GS)** através de um backend `pddi` especializado e `gsKit`:

- **Varredura Progressiva Nativa a 480p**: Saída progressiva nativa a $640 \times 480$ (Modo 80 a 59,94 Hz) com double-buffering.
- **Arquitetura Dedicada de VRAM em 16-bit**: Implementa framebuffers de 16 bits e modo de armazenamento de textura (`GS_PSM_CT16` / RGBA5551, 2 bytes/texel), liberando ~2,12 MiB de memória do GS. Camadas em tela cheia de $640 \times 360$ da interface (`background`, `jackie`, `jcslogo`), atlas de fontes e sistemas de partículas residem simultaneamente na VRAM com **zero degradação / zero expulsão por quadro**.
- **Renderização Sob Medida para o Hardware**: Rasterização por função fixa do GS substituindo os shaders programáveis do desktop, com filas de primitivas texturizadas e Gouraud, mistura alfa (alpha-blending) por hardware e reprodução de cores fiéis sem saturação excessiva (blowout).
- **Subsistemas Nativos**: Leitura de controles com `libpad`, inicialização de drivers IOP e transferências de dados DMA GIF.
- **Verificação**: Testado tanto no PCSX2 quanto em hardware real de PlayStation 2 via mapeamento de sistema de arquivos `host:`.

Para detalhes de compilação e desenvolvimento, consulte [ps2/README.md](ps2/README.md).

## Créditos & Reconhecimento

- **Criador Original do Projeto**: Sinceros agradecimentos e profundo reconhecimento a [SilverwireGames](https://github.com/SilverwireGames/ReChan) pelo grandioso trabalho original de engenharia reversa e reconstrução da base de código de *Jackie Chan Stuntmaster*.
- **Luckvice**: Criador e mantenedor deste fork, responsável pelas versões otimizadas, concepção da *RE:Chan HD Edition* para PSP, backend nativo Metal para macOS, porte nativo para PlayStation 2 (GS), otimizações profundas de memória/CPU e aprimoramentos para a experiência definitiva.
- **Comunidade & Contribuidores**: A todos os fãs, testadores e pesquisadores que colaboram com testes, relatórios de bugs, documentação e suporte para manter vivo o legado deste clássico.

## Aviso Legal e de Recursos

ReChan é um projeto não-oficial, gratuito e não-comercial de reimplementação de *Jackie Chan Stuntmaster*. Ele não é afiliado, endossado, patrocinado ou aprovado por qualquer desenvolvedor original, distribuidora ou detentor dos direitos autorais.

O pacote de lançamento pode incluir arquivos de suporte criados especificamente para o ReChan pelo autor do projeto, incluindo novos arquivos de texto, fontes e texturas. Esses arquivos foram criados do zero para o ReChan, não são arquivos do jogo de PlayStation e não contêm ou redistribuem conteúdo extraído ou copiado do jogo original.

Para que não restem dúvidas: **nenhum recurso original do jogo, imagem de disco, ROM, executável original, vídeo, música, efeito sonoro, textura, modelo, fase ou outros dados protegidos por direitos autorais do jogo de PlayStation são distribuídos com o ReChan.**
A imagem de disco inserida em `discimage` é fornecida pelo próprio usuário a partir de sua cópia legalmente obtida. Os recursos necessários são extraídos localmente no dispositivo do usuário e não fazem parte da distribuição do ReChan.

*Jackie Chan Stuntmaster* e todas as marcas registradas, direitos autorais, personagens, nomes, logotipos, recursos e dados do jogo permanecem propriedade de seus respectivos detentores. Bibliotecas de terceiros ou recursos incluídos com o ReChan permanecem sujeitos às suas respectivas licenças.

## Perguntas Frequentes (FAQ)

**Isso é um emulador de PS1?**  
Não. O ReChan é uma reimplementação. A lógica do jogo foi reescrita em C++ e não emula o hardware do PS1.

**Funcionará com as versões PAL?**  
Atualmente, apenas a versão NTSC-U (**SLUS-00684**) é suportada. A versão PAL não é suportada no momento e pode ter estruturas de dados diferentes.

**Haverá suporte ao hardware original do PS1?**  
Não. Isso não é um objetivo deste projeto.

**Posso contribuir?**  
Sim. Documentação, formatos de dados e análise de comportamento são muito bem-vindos. Consulte o [guia de compilação](docs/visionmaster/business/sources/BUILDING.md) para começar.

## Relatórios de Falhas (Crash reports)

Falhas críticas criam `minidumps/rechan-crash-<process-id>.txt` e instruem o usuário a abrir uma issue no GitHub. No Windows, também é gerado um minidump `.dmp` correspondente nessa pasta. Relatórios em modo de depuração anexam `rechan.log`; compilações de Release removem completamente os logs. Os relatórios são salvos localmente e nunca são enviados automaticamente.

Mantenedores podem verificar o manipulador de falhas com `--test-crash-reporter`. Defina `RECHAN_CRASH_REPORTER_NO_DIALOG=1` ao executar essa verificação em automação.

## Histórico de Alterações (Changelog)

As alterações notáveis estão listadas em [CHANGELOG.md](CHANGELOG.md).

## Licença
Licença MIT.

Aplica-se exclusivamente a esta base de código.  
Não se aplica ao jogo original nem aos seus recursos.
