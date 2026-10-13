# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog and this project adheres to Semantic Versioning.

## [Unreleased]

### Changed

- Créditos no PSP: entrada nos menus principais e rolagem com @gennariarmando, @GABRIELNNK e @luckvice e suas contribuições em 6 idiomas; aprovado pelo usuário. (doc hash 4d7f315916d3)

- UI/UX do PSP: HUD e menus legíveis em 480x272 (tarefa 002), menus sem opções de PC, estilo Moderno/Clássico e prompts com ícones PlayStation (△○×□, direcional aceso, selos START/SELECT/L/R); declarada completa pelo usuário. (doc hash f6b954172c4a)

- PSP a 60 FPS: 7 cenas a 58,9–60,05 FPS no PSP-3000 (hub 19,8 → 60 FPS, 623 → 76 draws/frame), VRAM 1,5 MB, áudio sem underruns; folga de CPU de 70% vira tarefa pós-beta. (doc hash 811dc6e58a12)

- Câmera: Original/80%/65%/50% com zoom em torno do Jackie, opção em Opções > Tela persistida em pspconf.ini; barras de inimigo limitadas abaixo do HUD; 58,5–60 fps em todos os níveis. Aprovado no PSP real. (doc hash 6b81e3f1a760)

- Modo cinema: 30 fps travados (swap a cada 2 refreshes, 1 tick de lógica por quadro), opção em Opções > Tela persistida em pspconf.ini; 29,8–30 fps, CPU ~33–48% por quadro; 60 fps inalterado. Aprovado no PSP real. (doc hash b266bbe60ef4)

- Entrega funcional encerrada por solicitação do usuário; testes e limitações registrados; desempenho PSP permanece na tarefa de otimização GU. (doc hash 38ca24106a8c)

- Entrega funcional encerrada por solicitação do usuário; testes e limitações registrados; desempenho PSP permanece na tarefa de otimização GU. (doc hash a3b78cdbf938)

- Entrega funcional encerrada por solicitação do usuário; testes e limitações registrados; desempenho PSP permanece na tarefa de otimização GU. (doc hash df7afbfdfb6a)

- Entrega funcional encerrada por solicitação do usuário; testes e limitações registrados; desempenho PSP permanece na tarefa de otimização GU. (doc hash b7aa13ff5248)

- Entrega funcional encerrada por solicitação do usuário; testes e limitações registrados; desempenho PSP permanece na tarefa de otimização GU. (doc hash e19a642b173e)

- Entrega funcional encerrada por solicitação do usuário; testes e limitações registrados; desempenho PSP permanece na tarefa de otimização GU. (doc hash 470bdd9d8ad1)

- Entrega funcional encerrada por solicitação do usuário; testes e limitações registrados; desempenho PSP permanece na tarefa de otimização GU. (doc hash d2f45045593c)

### Added

- Primeiro boot no PSP pede idioma e estilo do HUD (idioma do sistema pré-selecionado) e salva setup=1 em pspconf.ini; aprovado pelo usuário na Beta 1. (doc hash 73eef6f3e9be)

- FMVs no PSP: player próprio com streaming de setores, áudio XA e legendas em 6 idiomas; 9 vídeos até o fim no PSP-3000, atraso máximo 66 ms, RAM < 4 MB; aprovado pelo usuário na Beta 1. (doc hash b37fcf3399d4)

- PSP Edition Beta 1 publicada (pre-release v1.0.0-beta1_PSP): extrator de disco no PSP, pasta do jogo livre, versão no XMB e no selo, pacote sem arquivos do jogo; testada no PSP-3000 em instalação limpa. (doc hash 03c530efc2b0)

- HUD de PC como padrão no PSP: texto 14-18 px por strikes exatos com contorno embutido, barras modernas, hits acima do inimigo, barra do hub, tally e menus ampliados; opção Moderno/Clássico persistida (clássico com AdLib local); custo 0,73-0,81 ms (clássico 0,58-0,65) (doc hash d0315b9d32c3)

### Fixed

- Build desktop volta a compilar após o port PSP: pc/log.h só no PSP e PspPerf::Trace vazio fora dele; desktop EXIT=0, PSP e testes host inalterados. (doc hash 50fec5981d96)

- Fase 2 sem crash (buffer de geometria reservado, heap grande MEMSIZE=1, CRASH.TXT sem heap), efeitos sonoros restaurados (slots e cursores do mixer) e braços corretos (escala de UV); confirmado pelo usuário no PSP-3000. (doc hash cd36dce78ff9)

- Rastro do Jackie volta a clarear o fundo: semitransparência PSX modo 3 = destino + fonte/4 no renderer PSP; validado no PSP real (rel9) (doc hash 4ffbd4c90a31)
