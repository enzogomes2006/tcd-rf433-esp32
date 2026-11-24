# Relatório técnico — TCD

IFMT · Campus Cuiabá – Octayde Jorge da Silva  
Transmissão e Comunicação de Dados · Professor Ruy Barbosa  
Equipe: Enzo Aurélio, Felipe Enzo e Enzo Cristian

O [README principal](../README.md) contém a documentação completa do projeto e o roteiro de reprodução. O relatório revisado em LaTeX, com 28 páginas, foi preparado pela equipe e reserva espaço para T12.

## Documento técnico

| Documento | Formato | Situação |
| --- | --- | --- |
| [**Transmissão confiável de textos e imagens via rádio de 433 MHz entre dois ESP32**](Relatorio_TCD_Preliminar.pdf) | PDF · 28 páginas | Versão preliminar |

**[Abrir relatório em PDF](Relatorio_TCD_Preliminar.pdf)**

O documento está disponível para acompanhamento e consulta. A identificação da capa, o registro do T12 e a revisão final pela equipe continuam pendentes. O conteúdo do PDF foi preservado integralmente.

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

- [Relatório técnico em PDF](Relatorio_TCD_Preliminar.pdf): versão preliminar disponível.
- [Acervo de imagens](imagens/README.md): fotos, esquemáticos, antenas e imagens do projeto.
- [Evidências experimentais](evidencias/README.md): capturas seriais e fotografias organizadas por teste.
- `latex/`: fonte editável e figuras do Overleaf, ainda não publicadas.

A versão final do relatório será disponibilizada após a revisão e complementação da equipe.

## T12 — registro reservado

Executar o roteiro da seção 7 do README principal. Guardar logs de envio/recepção, CRC32, SHA-256 dos pixels nas duas pontas, PBM de referência, arquivos reconstruídos e fotografia do OLED. Registrar duração e contadores quando medidos. Preencher o espaço reservado no relatório após o ensaio.

## Revisão de referência do código

[35d4604](https://github.com/enzogomes2006/tcd-rf433-esp32/commit/35d4604424820aa82c8c3423637b6a23ae0019b2) — os dois sketches V7 e os dois scripts Python permanecem inalterados.
