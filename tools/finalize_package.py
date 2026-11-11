from pathlib import Path
p=Path('docs/BUILD.md');s=p.read_text(encoding='utf-8-sig');s=s.replace('## 测试工具','图像对比测试另需 Pillow（python -m pip install Pillow）；C++ 构建和标准库 WebSocket 客户端不依赖 Pillow。\n\n## 测试工具');p.write_text(s,encoding='utf-8')
p=Path('tools/package_release.py');s=p.read_text(encoding='utf-8-sig').replace("'cleanup_tests.py']","'cleanup_tests.py','package_release.py']");p.write_text(s,encoding='utf-8')
