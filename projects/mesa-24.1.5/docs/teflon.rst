TensorFlow Lite delegate
========================

Mesa contains a TensorFlow Lite delegate that can make use of NPUs to accelerate ML inference. It is implemented in the form of a *external delegate*, a shared library that the TensorFlow Lite runtime can load at startup. See https://www.tensorflow.org/api_docs/python/tf/lite/experimental/load_delegate.

.. list-table:: Supported acceleration hardware
   :header-rows: 1

   * - Gallium driver
     - NPU supported
     - Hardware tested
   * - Etnaviv
     - ``VeriSilicon VIPNano-QI.7120``
     - ``Amlogic A311D on Libre Computer AML-A311D-CC Alta and Khadas VIM3``

.. list-table:: Tested models
   :header-rows: 1

   * - Model name
     - Data type
     - Link (may be outdated)
     - Status
     - Inference speed on AML-A311D-CC Alta
   * - MobileNet V1
     - UINT8
     - http://download.tensorflow.org/models/mobilenet_v1_2018_08_02/mobilenet_v1_1.0_224_quant.tgz
     - Fully supported
     - ~15 ms
   * - MobileNet V2
     - UINT8
     - https://storage.googleapis.com/mobilenet_v2/checkpoints/quantized_v2_224_100.tgz
     - Fully supported
     - ~15.5 ms
   * - SSDLite MobileDet
     - UINT8
     - https://raw.githubusercontent.com/google-coral/test_data/master/ssdlite_mobiledet_coco_qat_postprocess.tflite
     - Fully supported
     - ~53 ms

Build
-----

Build Mesa as usual, with the -Dteflon=true argument.

Example instructions:

.. code-block:: console

   # Install build dependencies
   ~ # apt-get -y build-dep mesa
   ~ # apt-get -y install git cmake

   # Download sources
   ~ $ git clone https://gitlab.freedesktop.org/mesa/mesa.git

   # Build Mesa
   ~ $ cd mesa
   mesa $ meson setup build -Dgallium-drivers=etnaviv -Dvulkan-drivers= -Dteflon=true
   mesa $ meson compile -C build

Install runtime dependencies
----------------------------

Your board should have booted into a mainline 6.7 or greater kernel and have the etnaviv driver loaded. You will also need to enable the NPU device in the device tree by means of an overlay or by a change such as the below (and rebuild the DTB):

.. code-block:: diff

   diff --git a/arch/arm64/boot/dts/amlogic/meson-g12b-a311d-khadas-vim3.dts b/arch/arm64/boot/dts/amlogic/meson-g12b-a311d-khadas-vim3.dts
   index 4aa2b20bfbf2..4e8266056bca 100644
   --- a/arch/arm64/boot/dts/amlogic/meson-g12b-a311d-khadas-vim3.dts
   +++ b/arch/arm64/boot/dts/amlogic/meson-g12b-a311d-khadas-vim3.dts
   @@ -50,6 +50,10 @@ galcore {
         };
   };
   
   +&npu {
   +       status = "okay";
   +};
   +
   /*
   * The VIM3 on-board  MCU can mux the PCIe/USB3.0 shared differential
   * lines using a FUSB340TMX USB 3.1 SuperSpeed Data Switch between


.. code-block:: console

   # Install Python 3.10 and dependencies (as root)
   ~ # echo deb-src http://deb.debian.org/debian testing main >> /etc/apt/sources.list
   ~ # echo deb http://deb.debian.org/debian unstable main >> /etc/apt/sources.list
   ~ # echo 'APT::Default-Release "testing";' >> /etc/apt/apt.conf
   ~ # apt-get update
   ~ # apt-get -y install python3.10 python3-pytest python3-exceptiongroup

   # Install TensorFlow Lite Python package (as non-root)
   ~ $ python3.10 -m pip install --break-system-packages tflite-runtime==2.13.0

Do some inference with MobileNetV1
----------------------------------

.. code-block:: console

   ~ $ cd mesa/
   mesa $ TEFLON_DEBUG=verbose ETNA_MESA_DEBUG=ml_dbgs python3.10 src/gallium/frontends/teflon/tests/classification.py -i ~/tensorflow/assets/grace_hopper.bmp -m src/gallium/targets/teflon/tests/mobilenet_v1_1.0_224_quant.tflite -l src/gallium/frontends/teflon/tests/labels_mobilenet_quant_v1_224.txt -e build/src/gallium/targets/teflon/libteflon.so

   Loading external delegate from build/src/gallium/targets/teflon/libteflon.so with args: {}
   Teflon delegate: loaded etnaviv driver

   teflon: compiling graph: 89 tensors 28 operations
   idx scale     zp has_data size        
   =======================================
   0 0.023528   0 no       1x1x1x1024
   1 0.166099  42 no       1x1x1x1001
   2 0.000117   0 yes      1001x0x0x0
   3 0.004987  4a yes      1001x1x1x1024
   4 0.166099  42 no       1x1001x0x0
   5 0.166099  42 yes      2x0x0x0
   6 0.000171   0 yes      32x0x0x0
   7 0.023528   0 no       1x112x112x32
   8 0.021827  97 yes      32x3x3x3
   9 0.023528   0 no       1x14x14x512
   ...

   idx type    in out  operation type-specific
   ================================================================================================
   0 CONV    88   7  w: 8 b: 6 stride: 2 pad: SAME
   1 DWCONV   7  33  w: 35 b: 34 stride: 1 pad: SAME
   2 CONV    33  37  w: 38 b: 36 stride: 1 pad: SAME
   3 DWCONV  37  39  w: 41 b: 40 stride: 2 pad: SAME
   4 CONV    39  43  w: 44 b: 42 stride: 1 pad: SAME
   5 DWCONV  43  45  w: 47 b: 46 stride: 1 pad: SAME
   6 CONV    45  49  w: 50 b: 48 stride: 1 pad: SAME
   7 DWCONV  49  51  w: 53 b: 52 stride: 2 pad: SAME
   8 CONV    51  55  w: 56 b: 54 stride: 1 pad: SAME
   9 DWCONV  55  57  w: 59 b: 58 stride: 1 pad: SAME
   10 CONV    57  61  w: 62 b: 60 stride: 1 pad: SAME
   11 DWCONV  61  63  w: 65 b: 64 stride: 2 pad: SAME
   12 CONV    63  67  w: 68 b: 66 stride: 1 pad: SAME
   13 DWCONV  67  69  w: 71 b: 70 stride: 1 pad: SAME
   14 CONV    69  73  w: 74 b: 72 stride: 1 pad: SAME
   15 DWCONV  73  75  w: 77 b: 76 stride: 1 pad: SAME
   16 CONV    75  79  w: 80 b: 78 stride: 1 pad: SAME
   17 DWCONV  79  81  w: 83 b: 82 stride: 1 pad: SAME
   18 CONV    81  85  w: 86 b: 84 stride: 1 pad: SAME
   19 DWCONV  85   9  w: 11 b: 10 stride: 1 pad: SAME
   20 CONV     9  13  w: 14 b: 12 stride: 1 pad: SAME
   21 DWCONV  13  15  w: 17 b: 16 stride: 1 pad: SAME
   22 CONV    15  19  w: 20 b: 18 stride: 1 pad: SAME
   23 DWCONV  19  21  w: 23 b: 22 stride: 2 pad: SAME
   24 CONV    21  25  w: 26 b: 24 stride: 1 pad: SAME
   25 DWCONV  25  27  w: 29 b: 28 stride: 1 pad: SAME
   26 CONV    27  31  w: 32 b: 30 stride: 1 pad: SAME
   27 POOL    31   0  filter: 0x0 stride: 0 pad: VALID

   teflon: compiled graph, took 10307 ms
   teflon: invoked graph, took 21 ms
   teflon: invoked graph, took 17 ms
   teflon: invoked graph, took 17 ms
   teflon: invoked graph, took 17 ms
   teflon: invoked graph, took 16 ms
   0.866667: military uniform
   0.031373: Windsor tie
   0.015686: mortarboard
   0.007843: bow tie
   0.007843: academic
