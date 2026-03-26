#!/usr/bin/env python3
"""
Test the quantized TFLite model with the same data used in firmware.
This allows us to verify the model works correctly before testing on hardware.
"""

import re
import numpy as np
import tensorflow as tf
import sys
import os
import argparse

def extract_test_data_from_header(header_path):
    """Extract test arrays from C header file."""
    with open(header_path, 'r') as f:
        content = f.read()

    def extract_array(name):
        pattern = rf'const int8_t test_{name}_data\[128\]\[3\] = \{{(.*?)\}};'
        match = re.search(pattern, content, re.DOTALL)
        if not match:
            return None

        data_str = match.group(1)
        numbers = re.findall(r'-?\d+', data_str)
        numbers = [int(n) for n in numbers]
        arr = np.array(numbers, dtype=np.int8).reshape(128, 3)
        return arr

    return {
        'wing': extract_array('wing'),
        'ring': extract_array('ring'),
        'negative': extract_array('negative'),
    }

def test_model(model_path, test_data_dict=None, header_path=None):
    """Test the quantized model with our test data."""

    # Try to extract from header file first
    if test_data_dict is None:
        if header_path is None:
            # Default to wand/test_gestures.h relative to script location
            script_dir = os.path.dirname(os.path.abspath(__file__))
            header_path = os.path.join(script_dir, '..', 'wand', 'test_gestures.h')
        try:
            test_data_dict = extract_test_data_from_header(header_path)
            print(f"✓ Loaded test data from: {header_path}\n")
        except Exception as e:
            print(f"FAILED TO LOAD DATA: {e}")

    print("=" * 60)
    print("Testing TFLite Model")
    print("=" * 60)
    print(f"Model: {model_path}\n")

    # Load the interpreter
    try:
        interpreter = tf.lite.Interpreter(model_path=model_path)
        interpreter.allocate_tensors()
    except Exception as e:
        print(f"Error loading model: {e}")
        return False

    # Get input and output details
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]

    print("Model Information:")
    print(f"  Input shape: {input_details['shape']}")
    print(f"  Input dtype: {input_details['dtype']}")
    print(f"  Input quantization:")
    print(f"    scale: {input_details['quantization_parameters']['scales']}")
    print(f"    zero_point: {input_details['quantization_parameters']['zero_points']}")
    print(f"  Output shape: {output_details['shape']}")
    print(f"  Output dtype: {output_details['dtype']}")
    print(f"  Output quantization:")
    print(f"    scale: {output_details['quantization_parameters']['scales']}")
    print(f"    zero_point: {output_details['quantization_parameters']['zero_points']}")
    print()

    # Gesture names
    gesture_names = ['Wing', 'Ring', 'Slope', 'Negative']

    # Test data
    test_data = [
        ('WING', test_data_dict['wing'], 0),
        ('RING', test_data_dict['ring'], 1),
        ('NEGATIVE', test_data_dict['negative'], 3),
    ]

    all_correct = True

    for name, data, expected_label in test_data:
        if data is None:
            print(f"\n⚠ Skipping {name} - data not found")
            continue

        print(f"\n{'=' * 60}")
        print(f"Testing: {name}")
        print(f"{'=' * 60}")

        # Reshape to model input format [1, 128, 3, 1]
        input_tensor = data.reshape(1, 128, 3, 1)

        print(f"Input data shape: {input_tensor.shape}")
        print(f"Input data dtype: {input_tensor.dtype}")
        print(f"Input data range: [{input_tensor.min()}, {input_tensor.max()}]")

        # Run inference
        interpreter.set_tensor(input_details['index'], input_tensor)
        interpreter.invoke()
        output = interpreter.get_tensor(output_details['index'])

        # Get prediction
        predicted_class = np.argmax(output[0])
        predicted_score = output[0][predicted_class]

        print(f"\nRaw output scores (int8): {output[0]}")
        print(f"  Wing:     {output[0][0]:4d}")
        print(f"  Ring:     {output[0][1]:4d}")
        print(f"  Slope:    {output[0][2]:4d}")
        print(f"  Negative: {output[0][3]:4d}")

        print(f"\nPredicted: {gesture_names[predicted_class]} (class {predicted_class})")
        print(f"Expected:  {gesture_names[expected_label]} (class {expected_label})")
        print(f"Score:     {predicted_score}")

        if predicted_class == expected_label:
            print("✓ CORRECT!")
        else:
            print("✗ INCORRECT!")
            all_correct = False

    print(f"\n{'=' * 60}")
    if all_correct:
        print("ALL TESTS PASSED! ✓")
        print("The C code should produce identical results!")
    else:
        print("SOME TESTS FAILED! ✗")
    print(f"{'=' * 60}\n")

    return all_correct


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Test TFLite model with gesture data')
    parser.add_argument('model', nargs='?', 
                       help='Path to TFLite model file')
    parser.add_argument('--header', '-d', 
                       help='Path to test_gestures.h header file')
    args = parser.parse_args()

    # Default model path relative to script location
    if args.model is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        args.model = os.path.join(script_dir, '..', 'wand', 'magic_wand_model_quantized.tflite')

    success = test_model(args.model, header_path=args.header)
    sys.exit(0 if success else 1)
