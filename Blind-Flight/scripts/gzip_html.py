import gzip
import os

Import("env")

web_dir = os.path.join(env["PROJECT_DIR"], "web")
src_dir = os.path.join(env["PROJECT_DIR"], "src")
html_path = os.path.join(web_dir, "phone_ui.html")
header_path = os.path.join(src_dir, "phone_ui_gz.h")

if os.path.exists(html_path):
    html_mtime = os.path.getmtime(html_path)
    needs_update = True

    if os.path.exists(header_path):
        header_mtime = os.path.getmtime(header_path)
        if header_mtime >= html_mtime:
            needs_update = False

    if needs_update:
        with open(html_path, "rb") as f:
            raw = f.read()

        gz = gzip.compress(raw, compresslevel=9)

        with open(header_path, "w") as f:
            f.write("#pragma once\n")
            f.write("#include <Arduino.h>\n\n")
            f.write("// Auto-generated from web/phone_ui.html — do not edit\n")
            f.write("// Raw: %d bytes, Gzipped: %d bytes (%.0f%% reduction)\n\n"
                    % (len(raw), len(gz), (1 - len(gz) / len(raw)) * 100))
            f.write("static const uint8_t PAGE_HTML_GZ[] PROGMEM = {\n")
            for i in range(0, len(gz), 16):
                chunk = gz[i:i+16]
                f.write("    " + ", ".join("0x%02x" % b for b in chunk) + ",\n")
            f.write("};\n\n")
            f.write("#define PAGE_HTML_GZ_LEN %d\n" % len(gz))

        print("Gzipped phone_ui.html: %d -> %d bytes (%.0f%% reduction)"
              % (len(raw), len(gz), (1 - len(gz) / len(raw)) * 100))
else:
    print("WARNING: web/phone_ui.html not found, skipping gzip")
