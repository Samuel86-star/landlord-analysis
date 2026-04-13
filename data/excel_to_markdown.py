#!/usr/bin/env python3
import pandas as pd
import sys
import os

def excel_to_markdown(input_file):
    # 读取 Excel 文件，将所有列作为字符串处理，避免科学计数法
    df = pd.read_excel(input_file, dtype=str)

    # 使用 to_markdown 需要安装 tabulate 库
    markdown_content = df.to_markdown(index=False)

    # 生成输出文件名（将 .md 文件保存到 md 目录）
    base_name = os.path.splitext(os.path.basename(input_file))[0]
    output_file = os.path.join('md', f'{base_name}.md')

    # 确保 md 目录存在
    os.makedirs('md', exist_ok=True)

    # 写入 Markdown 文件
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(markdown_content)

    print(f"Excel 内容已转换并保存到: {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法: python excel_to_markdown.py <excel文件路径>")
        print("示例: python excel_to_markdown.py excel/data.xlsx")
        sys.exit(1)

    input_file = sys.argv[1]
    excel_to_markdown(input_file)
