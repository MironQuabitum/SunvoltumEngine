import os
import sys
import struct
import urllib.request
import zipfile
import subprocess

TEXCONV_EXE = "texconv.exe"
ZIP_URL = "https://github.com/microsoft/DirectXTex/releases/download/jun2024/DirectXTex-media-jun2024.zip"

# Маппинг DXGI-форматов
DXGI_FORMATS = {
    0: "UNKNOWN",
    2: "R32G32B32A32_FLOAT",
    10: "R16G16B16A16_FLOAT",
    28: "R8G8B8A8_UNORM",
    29: "R8G8B8A8_UNORM_SRGB",
    61: "R8_UNORM",
    71: "BC1_UNORM",             # DXT1
    72: "BC1_UNORM_SRGB",        
    74: "BC2_UNORM",             # DXT3
    75: "BC2_UNORM_SRGB",
    77: "BC3_UNORM",             # DXT5
    78: "BC3_UNORM_SRGB",
    80: "BC4_UNORM",             # ATI1
    81: "BC4_SNORM",
    83: "BC5_UNORM",             # ATI2 / Normal Map
    84: "BC5_SNORM",
    87: "B8G8R8A8_UNORM",        # BGRA
    91: "B8G8R8A8_UNORM_SRGB",
    95: "BC6H_UF16",
    96: "BC6H_SF16",
    98: "BC7_UNORM",             # BC7
    99: "BC7_UNORM_SRGB",
}

def ensure_texconv():
    """Скачивает и проверяет texconv.exe."""
    if os.path.exists(TEXCONV_EXE) and os.path.getsize(TEXCONV_EXE) < 100000:
        os.remove(TEXCONV_EXE)

    if not os.path.exists(TEXCONV_EXE):
        zip_path = "texconv_temp.zip"
        print("[+] Скачивание texconv.exe от Microsoft...")
        try:
            urllib.request.urlretrieve(ZIP_URL, zip_path)
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                for member in zip_ref.namelist():
                    if member.endswith("texconv.exe"):
                        with zip_ref.open(member) as source, open(TEXCONV_EXE, "wb") as target:
                            target.write(source.read())
                        break
            if os.path.exists(zip_path):
                os.remove(zip_path)
            print("[+] texconv.exe успешно установлен.\n")
        except Exception as e:
            if os.path.exists(zip_path):
                os.remove(zip_path)
            print(f"[!] Ошибка скачивания texconv.exe: {e}")
            sys.exit(1)

def inspect_dds(file_path):
    """Парсит заголовок DDS файла и возвращает параметры."""
    if not os.path.isfile(file_path):
        return None, f"Файл {file_path} не найден."

    try:
        with open(file_path, "rb") as f:
            magic = f.read(4)
            if magic != b"DDS ":
                return None, "Файл не является валидным DDS файлом."

            header_bytes = f.read(124)
            if len(header_bytes) < 124:
                return None, "Поврежден заголовок DDS."

            # Распаковка DDS_HEADER
            height, width, pitch, depth, mip_count = struct.unpack("<IIIII", header_bytes[8:28])
            
            # Чтение структуры DDPIXELFORMAT (начиная с байта 72 в DDS_HEADER)
            pf_flags, pf_fourcc = struct.unpack("<II", header_bytes[76:84])

            dxgi_format = "UNKNOWN"
            is_dx10 = False

            # Если FourCC равен "DX10" (0x30315844)
            if pf_fourcc == 0x30315844:
                is_dx10 = True
                dx10_bytes = f.read(20)
                if len(dx10_bytes) == 20:
                    dxgi_id = struct.unpack("<I", dx10_bytes[0:4])[0]
                    dxgi_format = DXGI_FORMATS.get(dxgi_id, f"DXGI_FORMAT_{dxgi_id}")
            else:
                # Чтение классического FourCC
                fourcc_str = pf_fourcc.to_bytes(4, byteorder='little').decode('ascii', errors='ignore').strip('\x00')
                if fourcc_str == "DXT1":
                    dxgi_format = "BC1_UNORM"
                elif fourcc_str == "DXT3":
                    dxgi_format = "BC2_UNORM"
                elif fourcc_str == "DXT5":
                    dxgi_format = "BC3_UNORM"
                elif fourcc_str in ["ATI1", "BC4U"]:
                    dxgi_format = "BC4_UNORM"
                elif fourcc_str in ["ATI2", "BC5U", "3DC1"]:
                    dxgi_format = "BC5_UNORM"
                elif pf_flags & 0x41: # DDPF_RGB / DDPF_ALPHAPIXELS
                    dxgi_format = "R8G8B8A8_UNORM"
                else:
                    dxgi_format = f"FourCC({fourcc_str})" if fourcc_str else "R8G8B8A8_UNORM"

            info = {
                "file": os.path.basename(file_path),
                "width": width,
                "height": height,
                "mipmaps": mip_count if mip_count > 0 else 1,
                "format": dxgi_format,
                "header_type": "DX10 Header" if is_dx10 else "Legacy DDS Header"
            }
            return info, None

    except Exception as e:
        return None, f"Ошибка чтения файла: {e}"

def convert_png(input_path, output_dir=None, fmt="BC7_UNORM"):
    """Конвертация PNG в DDS."""
    ensure_texconv()

    if not os.path.exists(input_path):
        print(f"[!] Файл или директория '{input_path}' не найдена.")
        return

    out_arg = output_dir if output_dir else (input_path if os.path.isdir(input_path) else os.path.dirname(input_path) or ".")

    cmd = [
        os.path.abspath(TEXCONV_EXE),
        "-ft", "dds",
        "-f", fmt,
        "-m", "0",
        "-y",
        "-o", out_arg
    ]

    if os.path.isfile(input_path):
        cmd.append(input_path)
    else:
        png_files = [os.path.join(input_path, f) for f in os.listdir(input_path) if f.lower().endswith('.png')]
        if not png_files:
            print("[!] PNG файлы не найдены.")
            return
        cmd.extend(png_files)

    print(f"[+] Конвертируем в {fmt}...")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode == 0:
        print("[+] Конвертация успешно завершена.")
    else:
        print(f"[!] Ошибка:\n{res.stderr or res.stdout}")

def print_help():
    print("""
Использование:
  1. Чтение информации о DDS файле:
     python dds_tool.py -info <путь_к_файлу.dds> [-command]

  2. Конвертация PNG в DDS:
     python dds_tool.py <путь_к_png_или_папке> [-f ФОРМАТ] [-o ВЫХОДНАЯ_ПАПКА]

Примеры:
  python dds_tool.py -info texture.dds
  python dds_tool.py -info texture.dds -command
  python dds_tool.py my_image.png -f BC3_UNORM
  python dds_tool.py C:\\Textures -f BC7_UNORM -o C:\\OutDDS
""")

def main():
    args = sys.argv[1:]
    if not args or "-h" in args or "--help" in args:
        print_help()
        return

    # Режим -info
    if "-info" in args:
        try:
            info_idx = args.index("-info")
            file_path = args[info_idx + 1]
        except IndexError:
            print("[!] Ошибка: Не указан DDS файл после -info")
            return

        show_command = "-command" in args

        info, error = inspect_dds(file_path)
        if error:
            print(f"[!] {error}")
            return

        print("=" * 45)
        print(f" Информация о DDS файле: {info['file']}")
        print("=" * 45)
        print(f" Разрешение    : {info['width']}x{info['height']}")
        print(f" Формат текстуры: {info['format']}")
        print(f" MIP-карты      : {info['mipmaps']}")
        print(f" Тип заголовка  : {info['header_type']}")

        if show_command:
            print("-" * 45)
            print("Параметры для повторного создания такого формата:")
            cmd_params = f"-f {info['format']} -m {info['mipmaps']} -w {info['width']} -h {info['height']}"
            print(f"Команда: python dds_tool.py <исходный_файл.png> {cmd_params}")
            print(f"Чистый texconv: texconv.exe -ft dds -f {info['format']} -m {info['mipmaps']} -w {info['width']} -h {info['height']} <файл>")
        print("=" * 45)
        return

    # Режим Конвертации (PNG -> DDS)
    input_path = args[0]
    
    fmt = "BC7_UNORM"
    if "-f" in args:
        fmt_idx = args.index("-f")
        if fmt_idx + 1 < len(args):
            fmt = args[fmt_idx + 1]

    out_dir = None
    if "-o" in args:
        out_idx = args.index("-o")
        if out_idx + 1 < len(args):
            out_dir = args[out_idx + 1]

    convert_png(input_path, out_dir, fmt)

if __name__ == "__main__":
    main()