These headers have been copied from TensorFlow 2.13.0.

To update the files to those in newer versions of TensorFlow:

cd $TENSORFLOW_CHECKOUT
cp --parents tensorflow/lite/builtin_ops.h $MESA_DIR/include/.
cp --parents tensorflow/lite/c/common.h $MESA_DIR/include/.
cp --parents tensorflow/lite/c/c_api.h $MESA_DIR/include/.
cp --parents tensorflow/lite/core/async/c/types.h $MESA_DIR/include/.
cp --parents tensorflow/lite/core/c/builtin_op_data.h $MESA_DIR/include/.
cp --parents tensorflow/lite/core/c/c_api.h $MESA_DIR/include/.
cp --parents tensorflow/lite/core/c/c_api_experimental.h $MESA_DIR/include/.
cp --parents tensorflow/lite/core/c/c_api_opaque.h $MESA_DIR/include/.
cp --parents tensorflow/lite/core/c/c_api_types.h $MESA_DIR/include/.
cp --parents tensorflow/lite/core/c/common.h $MESA_DIR/include/.
