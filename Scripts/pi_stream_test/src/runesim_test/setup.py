from setuptools import setup
setup(name='runesim_test', version='0.1.0', packages=['runesim_test'],
      data_files=[('share/ament_index/resource_index/packages', ['resource/runesim_test']),
                  ('share/runesim_test', ['package.xml'])],
      install_requires=['setuptools'], zip_safe=True,
      entry_points={'console_scripts': ['stream_viewer = runesim_test.viewer:main',
                                      'ps5_ptz_teleop = runesim_test.gamepad:main',
                                      'ptz_image_bridge = runesim_test.image_bridge:main']})
