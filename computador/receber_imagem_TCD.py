#!/usr/bin/env python3
"""TCD V7: registra imagem reconstruida no PC receptor via USB.

Instale: python -m pip install pyserial pillow
Uso: python receber_imagem_TCD.py COM5
Inicie este programa ANTES de enviar a imagem no PC A.
Arquivo PBM P4 sempre salvo; PNG salvo se Pillow estiver instalado.
"""
import argparse
import hashlib
import re
import sys
import time
import zlib
from datetime import datetime
from pathlib import Path

try:
    import serial
except ImportError:
    raise SystemExit("Falta pyserial. Instale: python -m pip install pyserial pillow")

try:
    from PIL import Image
except ImportError:
    Image = None


class Receptor:
    def __init__(self, pasta: Path):
        self.pasta = pasta
        self.reset()

    def reset(self):
        self.buffer = None
        self.width = 0
        self.height = 0
        self.expected = 0
        self.crc = 0

    def processar(self, linha: str):
        if linha.startswith("@IMG_BEGIN "):
            partes = linha.split()
            if len(partes) != 5:
                print("CABECALHO IMG invalido:", linha)
                self.reset()
                return
            try:
                w, h, total = (int(x) for x in partes[1:4])
                crc = int(partes[4], 16)
                if w not in (32,64,128) or h not in (16,32,64) or total != w*h//8:
                    raise ValueError("dimensoes/tamanho inesperados")
            except ValueError as exc:
                print("ERRO no header:", exc)
                self.reset()
                return
            self.width, self.height, self.expected, self.crc = w, h, total, crc
            self.buffer = bytearray()
            print(f"Recebendo arquivo reconstruido: {w}x{h}, {total} bytes")
        elif linha.startswith("@IMG_DATA "):
            if self.buffer is None:
                return
            hexd = linha[len("@IMG_DATA "):].strip()
            if not re.fullmatch(r"[0-9a-fA-F]{32}", hexd):
                print("ERRO: bloco hexadecimal invalido")
                self.reset()
                return
            self.buffer.extend(bytes.fromhex(hexd))
            if len(self.buffer) > self.expected:
                print("ERRO: bytes excedentes no arquivo")
                self.reset()
                return
            if len(self.buffer) % 128 == 0 or len(self.buffer) == self.expected:
                print(f"Recebidos {len(self.buffer)}/{self.expected} bytes")
        elif linha.startswith("@IMG_END "):
            if self.buffer is None:
                return
            try:
                crc_end = int(linha.split()[1], 16)
            except (ValueError, IndexError):
                print("ERRO: final invalido")
                self.reset()
                return
            conteudo = bytes(self.buffer)
            crc_lido = zlib.crc32(conteudo) & 0xFFFFFFFF
            if len(conteudo) != self.expected or crc_lido != self.crc or crc_lido != crc_end:
                print(f"ERRO DE INTEGRIDADE: {len(conteudo)}/{self.expected} bytes, "
                      f"CRC calculado {crc_lido:08X}, esperado {self.crc:08X}")
                self.reset()
                return
            self.pasta.mkdir(parents=True, exist_ok=True)
            agora = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
            nome = self.pasta / f"imagem_recebida_{self.width}x{self.height}_{agora}"
            pbm = nome.with_suffix(".pbm")
            pbm.write_bytes(f"P4\n{self.width} {self.height}\n".encode() + conteudo)
            print(f"\nIMAGEM COMPLETA E VALIDADA! CRC32={crc_lido:08X}")
            print(f"SHA256 dos pixels: {hashlib.sha256(conteudo).hexdigest()}")
            print(f"PBM salvo em: {pbm}")
            if Image:
                png = nome.with_suffix(".png")
                with Image.open(pbm) as imagem:
                    imagem.save(png)
                print(f"PNG salvo em: {png}")
            else:
                print("Instale pillow para salvar tambem em PNG.")
            self.reset()


def main():
    ap = argparse.ArgumentParser(description="Reconstroi arquivos recebidos via ESP32/RF433")
    ap.add_argument("porta", help="Porta USB do ESP32 receptor (ex.: COM5)")
    ap.add_argument("--saida", type=Path, default=Path("imagens_recebidas"))
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()
    receptor = Receptor(args.saida)
    try:
        with serial.Serial(args.porta, args.baud, timeout=0.5) as s:
            time.sleep(2.5)
            s.reset_input_buffer()
            print(f"Ouvindo ESP32 na porta {args.porta} (115200). Ctrl+C encerra.")
            print("Inicie agora o script de envio no computador A.")
            while True:
                raw = s.readline()
                if not raw:
                    continue
                linha = raw.decode("utf-8", errors="replace").strip()
                if not linha:
                    continue
                if linha.startswith("@IMG_"):
                    receptor.processar(linha)
                else:
                    print(linha, flush=True)
    except KeyboardInterrupt:
        print("\nEncerrado pelo usuario.")
        return 0
    except serial.SerialException as exc:
        print(f"Erro na porta serial: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
