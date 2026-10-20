# PrepareIdfProject.py, Version: 1.02
"""Keep IDF's generated project metadata and managed components inside the cache."""
from pathlib import Path
import sys
from SyncGeneratedFiles import write_text_if_changed

project=Path(sys.argv[1]).resolve()
output=Path(sys.argv[2]).resolve()
cmake=(project/'CMakeLists.txt').read_text()
needle='${CMAKE_CURRENT_LIST_DIR}/Sw/main'
assert cmake.count(needle)==1
write_text_if_changed(output/'CMakeLists.txt',
    '# Generated from the project CMakeLists.txt; do not edit.\n'+
    cmake.replace(needle,str(project/'Sw/main')))
defaults=(project/'sdkconfig.defaults').read_text()
import re
match=re.search(r'^CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="([^"]+)"$',defaults,re.M)
assert match is not None
needle=match[0]
csv=(project/match[1]).resolve()
assert csv.is_file()
write_text_if_changed(output/'sdkconfig.defaults',
    defaults.replace(needle,'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="'+str(csv)+'"'))
