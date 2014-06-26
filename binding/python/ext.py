from distutils.core import setup, Extension

xio = Extension('xio',
                define_macros = [('MAJOR_VERSION', '0'),
                                 ('MINOR_VERSION', '9')],
                include_dirs = ['/usr/local/include', '../../src', '../../include'],
                libraries = ['xio'],
                library_dirs = ['/usr/local/lib', '../../.libs'],
                sources = ['python_xio.c'])

setup (name = 'xio',
       version = '0.9',
       description = 'This is a proxyio package',
       author = 'DongFang',
       author_email = 'yp.fangdong@gmail.com',
       url = 'http://proxyio.org',
       ext_modules = [xio])
