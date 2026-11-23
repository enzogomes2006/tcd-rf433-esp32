#!/usr/bin/env python3
"""TCD V7: envia PNG/JPG/BMP/PBM por USB -> ESP32 -> RF433.

Instale: python -m pip install pyserial pillow
Uso: python enviar_imagem_TCD.py COM19 foto.png --size 64x32
     python enviar_imagem_TCD.py COM19 foto.jpg --size 128x64
Execute receptor Python em outro laptop ANTES deste programa.
"""
import argparse
import hashlib
import sys
import time
import zlib
from pathlib import Path

try:
    import serial
except ImportError:
    raise SystemExit("Falta pyserial. Instale com: python -m pip install pyserial pillow")
try:
    from PIL import Image, ImageOps
except ImportError:
    raise SystemExit("Falta Pillow. Instale com: python -m pip install pyserial pillow")

VALID_SIZES = {"32x16": (32, 16), "32x32": (32, 32), "64x16": (64, 16),
               "64x32": (64, 32), "64x64": (64, 64), "128x16": (128, 16),
               "128x32": (128, 32), "128x64": (128, 64)}


def converter_imagem(path: Path, width: int, height: int, dither: bool) -> bytes:
    # A tela OLED acende os pixels onde o bit=1. PBM P4 grava preto=1.
    # Assim, um desenho PRETO sobre fundo BRANCO vira desenho aceso na OLED.
    with Image.open(path) as imagem:
        imagem = ImageOps.exif_transpose(imagem).convert("L")
        ajustada = ImageOps.contain(imagem, (width, height), Image.Resampling.LANCZOS)
        tela = Image.new("L", (width, height), 255)
        tela.paste(ajustada, ((width - ajustada.width)//2,
                               (height - ajustada.height)//2))
    invertida = ImageOps.invert(tela)
    modo = Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE
    bits = invertida.convert("1", dither=modo).tobytes()
    assert len(bits) == width * height // 8
    return bits


def esperar(s, prefixo: str, timeout: float, valor_esperado=None):
    prazo = time.monotonic() + timeout
    while time.monotonic() < prazo:
        raw = s.readline()
        if not raw:
            continue
        linha = raw.decode("utf-8", errors="replace").strip()
        if not linha:
            continue
        if linha.startswith("@PC_ERROR"):
            raise RuntimeError(f"ESP32 respondeu: {linha}")
        if linha.startswith(prefixo):
            if valor_esperado is not None:
                got = linha[len(prefixo):].strip()
                if got != str(valor_esperado):
                    raise RuntimeError(f"Esperava {prefixo}{valor_esperado}, recebi {linha}")
            return linha
        # Imprime os logs de protocolo, para registrar os testes no relatório.
        print(linha, flush=True)
    raise TimeoutError(
        "O ESP32 nao respondeu ao comando. Pode estar retransmitindo "
        "por falta de ACK. Verifique o Circuito B e reinicie ambos "
        "se precisar cancelar a sessao."
    )


def enviar(s, comando: str):
    s.write((comando + "\n").encode("ascii"))
    s.flush()


def main():
    ap = argparse.ArgumentParser(description="Transmite imagem TCD V7 via ESP32/RF433")
    ap.add_argument("porta", help="Porta USB do ESP32 transmissor (ex.: COM19)")
    ap.add_argument("imagem", type=Path, help="Arquivo PNG, JPG, BMP ou PBM")
    ap.add_argument("--size", choices=sorted(VALID_SIZES), default="64x32",
                    help="Resolucao da imagem no RF (padrao 64x32)")
    ap.add_argument("--sem-dither", action="store_true",
                    help="Preto/branco sem reticulacao; melhor para logotipos")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()
    if not args.imagem.is_file():
        ap.error(f"Imagem nao encontrada: {args.imagem}")

    w, h = VALID_SIZES[args.size]
    dados = converter_imagem(args.imagem, w, h, not args.sem_dither)
    crc = zlib.crc32(dados) & 0xFFFFFFFF
    sha = hashlib.sha256(dados).hexdigest()
    referencia = args.imagem.with_name(args.imagem.stem + f"_enviada_{w}x{h}.pbm")
    referencia.write_bytes(f"P4\n{w} {h}\n".encode("ascii") + dados)
    print(f"Arquivo original: {args.imagem}")
    print(f"Imagem RF: {w}x{h}, {len(dados)} bytes, {len(dados)//16} blocos de 16")
    print(f"CRC32: {crc:08X} | SHA256 dos pixels: {sha}")
    print(f"Referencia salva: {referencia}")
    print("IMPORTANTE: deixe o script receptor rodando no outro computador.")

    try:
        with serial.Serial(args.porta, args.baud, timeout=0.5, write_timeout=5) as s:
            # Muitos ESP32 reiniciam ao abrir a porta USB.
            time.sleep(2.5)
            s.reset_input_buffer()
            enviar(s, f"/imgstart {w} {h} {crc:08X}")
            esperar(s, "@PC_READY", 240)
            for pos in range(0, len(dados), 16):
                bloco = dados[pos:pos+16]
                enviar(s, "/imghex " + bloco.hex().upper())
                esperar(s, "@PC_ACK ", 240, pos + len(bloco))
                if (pos + len(bloco)) % 64 == 0 or pos + len(bloco) == len(dados):
                    print(f"Enviado: {pos + len(bloco)}/{len(dados)} bytes", flush=True)
            enviar(s, "/imgend")
            esperar(s, "@PC_DONE ", 240, f"{crc:08X}")
            print("TRANSMISSAO RF CONFIRMADA. Verifique o arquivo salvo no computador B.")
    except (serial.SerialException, TimeoutError, RuntimeError) as erro:
        print(f"FALHA: {erro}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
