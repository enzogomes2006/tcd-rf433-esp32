# Relatório técnico — TCD

IFMT · Campus Cuiabá – Octayde Jorge da Silva  
Transmissão e Comunicação de Dados · Professor Ruy Barbosa  
Equipe: Enzo Aurélio, Felipe Enzo e Enzo Cristian

O [README principal](../README.md) contém a documentação completa do projeto e o roteiro de reprodução. O relatório revisado em LaTeX, com 28 páginas, foi preparado pela equipe e reserva espaço para T12.

## Acervo geral de imagens

[Consultar as categorias do acervo](imagens/README.md).

## Galeria por teste

Acesse a [galeria de evidências T01–T14](evidencias/README.md), organizada em pastas individuais.

## Estado experimental

| Ensaios | Situação | Evidências no acervo da equipe |
| --- | --- | --- |
| T01–T09 | Realizados | Capturas dos terminais |
| T10 e T11 | Aprovados funcionalmente | Relato, sem logs específicos |
| T12 | Pendente formalmente | A coletar |
| T13 | Aprovado funcionalmente | Fotografia do OLED e relato |
| T14 | Aprovado funcionalmente | Relato, sem logs específicos |

T08 e T09 possuem ressalvas de contadores acumulados. Não inventar valores ausentes. Consultar a seção 8 do README principal.

## Organização dos anexos

- `imagens/`: fotos de A e B, componentes, antenas, imagem de origem e T13.
- `logs/`: capturas seriais T01–T09 e registros posteriores.
- `Relatorio_TCD_Sem_Referencias.pdf`: revisão do PDF a anexar.
- `latex/`: fonte editável e figuras do projeto Overleaf a anexar.

A versão integral do PDF, os arquivos LaTeX e as evidências de alta resolução ainda precisam ser publicados nesta pasta.

## T12 — registro reservado

Executar o roteiro da seção 7 do README principal. Guardar logs de envio/recepção, CRC32, SHA-256 dos pixels nas duas pontas, PBM de referência, arquivos reconstruídos e fotografia do OLED. Registrar duração e contadores quando medidos. Preencher o espaço reservado no relatório após o ensaio.

## Revisão de referência do código

[35d4604](https://github.com/enzogomes2006/tcd-rf433-esp32/commit/35d4604424820aa82c8c3423637b6a23ae0019b2) — os dois sketches V7 e os dois scripts Python permanecem inalterados.
