from distutils.core import setup, Extension

def main():
    setup(name="yuv2rgb",
          version="1.0.0",
          description="Python interface for the yuv to rgb conversion routine",
          author="Phil Burgess & Ton van Overbeek",
          author_email="tvoverbeek@gmail.com",
          ext_modules=[Extension("yuv2rgb", ["yuv2rgb.c"])])

if __name__ == "__main__":
    main()
