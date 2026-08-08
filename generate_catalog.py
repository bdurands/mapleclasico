import xml.etree.ElementTree as ET
import os

xml_file = r"c:\Users\Nion\Desktop\mapleclasico\wz\Etc.wz\Commodity.img.xml"
out_file = r"c:\Users\Nion\Desktop\mapleclasico\cashshop\catalog-nuevo.tsv"

def generate():
    if not os.path.exists(xml_file):
        print(f"File not found: {xml_file}")
        return

    tree = ET.parse(xml_file)
    root = tree.getroot()

    seen_items = set()

    with open(out_file, 'w', encoding='utf-8') as f:
        f.write("# Cash Shop catalogue, generated from Etc.wz/Commodity.img.xml\n")
        f.write("# itemId\tprice\tcount\ttab\tcategory\tperiod\tgender\tname\n")
        
        for imgdir in root.findall('imgdir'):
            sn = 0
            item_id = 0
            price = 0
            count = 1
            period = 0
            gender = 2
            
            for child in imgdir:
                if child.tag == 'int':
                    name = child.get('name')
                    try:
                        value = int(child.get('value'))
                        if name == 'SN': sn = value
                        elif name == 'ItemId': item_id = value
                        elif name == 'Price': price = value
                        elif name == 'Count': count = value
                        elif name == 'Period': period = value
                        elif name == 'Gender': gender = value
                    except ValueError:
                        pass
            
            if sn > 0 and item_id > 0:
                if item_id in seen_items:
                    continue
                seen_items.add(item_id)

                tab = sn // 10000000
                category = (sn // 100000) % 100
                
                f.write(f"{item_id}\t{price}\t{count}\t{tab}\t{category}\t{period}\t{gender}\t\n")

    print(f"Generated successfully at: {out_file} with {len(seen_items)} unique items.")

if __name__ == '__main__':
    generate()
