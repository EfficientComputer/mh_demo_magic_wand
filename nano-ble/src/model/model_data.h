#ifndef EXPORT_MODEL_DATA_H_
#define EXPORT_MODEL_DATA_H_

// Exported model bytes (TensorFlow Lite flatbuffer).
// Provided via macro-renamed inclusion of original magic_wand_model_data.cpp
// to avoid duplicating a large hex array manually.
extern const unsigned char g_magic_wand_model_data[];
extern const int g_magic_wand_model_data_len; // size in bytes

#endif // EXPORT_MODEL_DATA_H_
