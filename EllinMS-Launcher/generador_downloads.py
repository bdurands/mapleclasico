import os
import hashlib
import xml.etree.ElementTree as ET
from xml.dom import minidom

# Configuración
SOURCE_DIR = r"C:\EllinMS"
OUTPUT_FILE = r"C:\EllinMS\downloads.xml"
BASE_URL = "https://cdn.ellinms.com/"

def calculate_md5(filepath):
    hash_md5 = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest().lower()

def main():
    if not os.path.exists(SOURCE_DIR):
        print(f"Error: El directorio {SOURCE_DIR} no existe.")
        return

    root = ET.Element("downloads")

    for root_dir, dirs, files in os.walk(SOURCE_DIR):
        for file in files:
            if file == "downloads.xml":
                continue
            
            filepath = os.path.join(root_dir, file)
            # Calcular ruta relativa
            rel_path = os.path.relpath(filepath, SOURCE_DIR).replace("\\", "/")
            size = os.path.getsize(filepath)
            
            # Calcular hash
            file_hash = calculate_md5(filepath)
            
            # URL final (reemplazando espacios por %20 si es necesario)
            url = BASE_URL + rel_path.replace(" ", "%20")
            
            file_elem = ET.SubElement(root, "file")
            
            ET.SubElement(file_elem, "file_name").text = rel_path
            ET.SubElement(file_elem, "file_hash").text = file_hash
            ET.SubElement(file_elem, "file_link").text = url
            ET.SubElement(file_elem, "file_size").text = str(size)

    # Formatear el XML para que sea legible
    xmlstr = minidom.parseString(ET.tostring(root)).toprettyxml(indent="    ")
    
    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        f.write(xmlstr)
        
    print(f"Generado exitosamente: {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
