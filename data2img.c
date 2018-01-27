#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// Imagemagick
#include <wand/MagickWand.h>

// Typedefs and configuration
typedef uint8_t byte;
typedef uint32_t uint;
const uint32_t LINELENGTH_DEFAULT = 256; // Length of line in pixels

// Calculate extended size of string
int calculateExtendedSize(uint inputSize) {
	if (inputSize >= 16777216) { // Error if greater than or equal to 16MB
		return -1;
	}
	uint totalSize = 3+inputSize+2;
	return (totalSize-totalSize%3)*8; // Calculate extended size of data (length header + input + padding)
}

// Extend a group of three bytes into 8 bytes, with 3 bits per pixel (R, G, and B). Each input byte gets its own color channel.
void extendByteGroup(byte in[3], byte rgb[24]) {
	for (byte i=0; i<3; i++) { // Iterate through bytes
		for (byte bit=0; bit<8; bit++) { // Iterate through bits of input byte
			rgb[i*8+bit] = ((in[i]>>bit)%2)*255; // Set each bit to one channel in the 8 RGB bytes
		}
	}
}

// Read data from buffer, write image data to buffer
int extendBuffer(byte* input, uint input_size, byte* output, uint output_size) {
	// Test validity of input
	if (input_size >= 16777216) {
		puts("This program does not support files larger than 16MB.");
		return 1;
	}
	if (output_size < calculateExtendedSize(input_size)) {
		puts("Output buffer is too small.");
		return 2;
	}
	// Allocate variables, write metadata
	byte datachunk[3];
	byte pixels[24]; // Datachunk is input data, pixels is output data in three channels
	datachunk[0] = input_size%256; datachunk[1] = (input_size>>8)%256; datachunk[2] = (input_size>>16)%256;
	extendByteGroup(datachunk, pixels); // First, write the size of the input file
	uint p=0; // Pixel data iterator
	for (p=0; p<24; p++) {
		output[p] = pixels[p];
	}
	// Iterate through input
	uint lastChunkPos = (input_size-1)-(input_size-1)%3;
	for (uint i=0; i<lastChunkPos; i+=3) {
		datachunk[0] = input[i]; datachunk[1] = input[i+1]; datachunk[2] = input[i+2];
		extendByteGroup(datachunk, pixels);
		for (uint ii=0; ii<24; ii++) {
			output[p+ii] = pixels[ii];
		} // Copy to position in output, plus 3 bytes for size header
		p += 24;
	}
	// Finalize last chunk (no need to zero out datachunk; everything after marked length doesn't matter
	switch (input_size-lastChunkPos) {
		case 3: datachunk[2] = input[lastChunkPos+2];
		case 2: datachunk[1] = input[lastChunkPos+1];
		case 1: datachunk[0] = input[lastChunkPos]; break;
		default: puts("WTF?"); return 4;
	}
	extendByteGroup(datachunk, pixels);
	for (uint ii=0; ii<24; ii++) {
		output[p+ii] = pixels[ii];
	} // Copy to position in output, plus 3 bytes for size header
	p += 24;
	// Pad rest of output with 0's
	for (p=p; p<output_size; p++) {
		output[p] = 0;
	}
	return 0;
}

// Contract a group of 8 bytes, with 3 bits per pixel (R, G, and B), into three bytes. Each output byte comes from its own color channel.
void contractByteGroup(byte rgb[24], byte out[3]) {
	out[0] = out[1] = out[2] = 0; // Initialize to 0
	for (byte i=0; i<3; i++) { // Iterate through bytes
		for (byte bit=0; bit<8; bit++) { // Iterate through bits of input byte
			out[i] |= (rgb[i*8+bit]>>7)<<bit; // Set the bit bit of out[i] to the most significant bit of the color channel
		}
	}
}

// Read image data from buffer, write data to buffer
int contractBuffer(byte* input, uint input_buffer_size, byte* output, uint output_size_max, uint* output_size_output) {
	// Check validity of input
	if (input_buffer_size < 25) {
		puts("Input buffer is too small.");
		return 1;
	}
	// Allocate variables and read metadata
	uint p=0; // Pixel data iterator
	byte datachunk[3];
	byte pixels[24]; // Datachunk is input data, pixels is output data in three channels
	for (p=0; p<24; p++) { pixels[p] = input[p]; } // Read size metadata from front of image
	contractByteGroup(pixels, datachunk);
	uint output_size = (uint)datachunk[0] + ((uint)datachunk[1]<<8) + ((uint)datachunk[2]<<16);
	uint input_size = calculateExtendedSize(output_size); // Real input size
	// Check validity of input now that we have output size and real input size
	if (input_buffer_size < input_size) {
		puts("Input buffer is too small.");
		return 1;
	}
	if (output_size_max < output_size) {
		puts("Output buffer is too small.");
		return 2;
	}
	// Iterate through input. This time, we can read from the padding, and therefore no special procedure is required for the last chunk
	for (uint i=0; i<output_size; i+=3) {
		for (uint ii=0; ii<24; ii++) {
			pixels[ii] = input[p+ii];
		}
		contractByteGroup(pixels, datachunk); // Copy input over to pixels and contract it into datachunk
		output[i] = datachunk[0]; output[i+1] = datachunk[1]; output[i+2] = datachunk[2]; // Copy datachunk over to output
		p += 24;
	}
	// Specify the size of the output and return success
	*output_size_output = output_size;
	return 0;
}

// Full function with image encoding
int data2img(FILE* input, const char* output, uint linelength) {
	// Get size of file
	fseek(input, 0L, SEEK_END);
	uint input_size = ftell(input);
	rewind(input);
	// Make sure file isn't too big, then allocate buffer for it
	if (input_size >= 16777216) {
		puts("This program does not support files larger than 16MB.");
		return 1;
	}
	byte* data = (byte*)malloc(input_size);
	uint data_size = input_size;
	// Transfer file to inputbuf
	fread(data, 1, input_size, input);
	// Allocate pixels
	uint imagedata_size = calculateExtendedSize(data_size);
	imagedata_size = imagedata_size-(imagedata_size%linelength)+linelength; // Output is of extended size padded to nearest LINELENGTH
	byte* imagedata = (byte*)malloc(imagedata_size);
	// Extend data into imagedata and free it
	int result = extendBuffer(data, data_size, imagedata, imagedata_size);
	if (result != 0) {
		free(data); return result;
	}
	free(data); data = NULL;
	// Write imagedata to output file as an image, then free imagedata
	MagickWand* magick_wand = NewMagickWand();
	MagickConstituteImage(magick_wand, linelength/3, imagedata_size/linelength, "RGB", CharPixel, imagedata);
	MagickWriteImage(magick_wand, output);
	free(imagedata); return 0;
}

// Full function with image decoding
int img2data(FILE* input, FILE* output) {
	// Get size of file
	fseek(input, 0L, SEEK_END);
	uint input_size = ftell(input);
	rewind(input);
	// Get attributes from file and decode image
	MagickWand* magick_wand = NewMagickWand();
	MagickReadImageFile(magick_wand, input);
	size_t image_width = MagickGetImageWidth(magick_wand), image_height = MagickGetImageHeight(magick_wand);
	uint imagedata_size = image_width*image_height*3;
	if (imagedata_size == 0) { return 127; } // Not an image file. Return 127.
	byte* imagedata = (byte*)malloc(imagedata_size); // Padding should have been included
	MagickExportImagePixels(magick_wand, 0, 0, image_width, image_height, "RGB", CharPixel, imagedata);
	// Contract imagedata and free it
	uint data_size_max = imagedata_size/8*3+256; // Size of imagedata divided by 8 times 3 plus some padding
	byte* data = (byte*)malloc(data_size_max);
	uint data_size; // This will be written to by the contractBuffer function
	int result = contractBuffer(imagedata, imagedata_size, data, data_size_max, &data_size);
	free(imagedata); imagedata = NULL;
	if (result != 0) { // If contraction failed, return failure
		free(data); return result;
	}
	// Write to output file, free data, and return
	fwrite(data, 1, data_size, output);
	free(data); return 0;
}

int main(int argc, char* argv[]) {
	// Check arguments
	if (argc < 4) {
		puts("Usage:");
		printf("\tEncoding: ./data2img encode [input] [output] [rowlength=%d]\n", LINELENGTH_DEFAULT);
		printf("\tDecoding: ./data2img decode [input] [output]\n");
		return 254;
	}
	// Initialize imagemagick and input file
	MagickWandGenesis();
	const char *inputFileName = argv[2], *outputFileName = argv[3];
	FILE* inputFile = fopen(inputFileName, "rb");
	if (inputFile == NULL) {
		puts("Error opening input file");
		return 128;
	}
	// Check if encoding or decoding, and act accordingly
	int result = 254;
	if (strcmp(argv[1], "encode") == 0) {
		// Get line length
		int linelength = LINELENGTH_DEFAULT*3; // Length of line in bytes = length of line in pixels*3
		if (argc > 4) {
			linelength = atoi(argv[4])*3; // Length of line in bytes = length of line in pixels*3
		}
		// Check line length
		if (linelength <= 0) {
			puts("Row length must be an integer greater than 0.");
			result = 254;
		} else {
			// Run data2img
			result = data2img(inputFile, outputFileName, linelength);
		}
	} else if (strcmp(argv[1], "decode") == 0) {
		// Initialize output file
		FILE* outputFile = fopen(outputFileName, "wb");
		if (outputFile == NULL) {
			puts("Error opening output file");
			result = 128;
		} else {
			// Run img2data and close outputFile
			result = img2data(inputFile, outputFile);
			fclose(outputFile);
		}
	} else {
		puts("Invalid Command.");
	}
	fclose(inputFile);
	return result;
}