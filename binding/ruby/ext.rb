require 'mkmf'

# Give it a name
extension_name = 'xio'

find_header ('xio/sp.h', '../../include')
find_library ('xio', 'sp_endpoint', '../../.libs')
have_library 'xio'

dir_config("xio")

create_makefile("xio")
