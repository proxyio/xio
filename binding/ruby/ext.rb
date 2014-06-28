require 'mkmf'

# Give it a name
extension_name = 'xio'

have_library 'xio'

dir_config("xio")

create_makefile("xio")
