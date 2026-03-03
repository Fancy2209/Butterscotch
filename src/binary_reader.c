#include "binary_reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BinaryReader BinaryReader_create(const uint8_t* buffer, size_t size) {
    return (BinaryReader){.buffer = buffer, .size = size, .position = 0};
}

static void boundsCheck(BinaryReader* reader, size_t bytes) {
    if (reader->position + bytes > reader->size) {
        fprintf(stderr, "BinaryReader: read overflow at position 0x%zX (requested %zu bytes, buffer size 0x%zX)\n", reader->position, bytes, reader->size);
        exit(1);
    }
}

uint8_t BinaryReader_readUint8(BinaryReader* reader) {
    boundsCheck(reader, 1);
    uint8_t value = reader->buffer[reader->position];
    reader->position += 1;
    return value;
}

int16_t BinaryReader_readInt16(BinaryReader* reader) {
    boundsCheck(reader, 2);
    int16_t value;
    memcpy(&value, reader->buffer + reader->position, 2);
    reader->position += 2;
    return value;
}

uint16_t BinaryReader_readUint16(BinaryReader* reader) {
    boundsCheck(reader, 2);
    uint16_t value;
    memcpy(&value, reader->buffer + reader->position, 2);
    reader->position += 2;
    return value;
}

int32_t BinaryReader_readInt32(BinaryReader* reader) {
    boundsCheck(reader, 4);
    int32_t value;
    memcpy(&value, reader->buffer + reader->position, 4);
    reader->position += 4;
    return value;
}

uint32_t BinaryReader_readUint32(BinaryReader* reader) {
    boundsCheck(reader, 4);
    uint32_t value;
    memcpy(&value, reader->buffer + reader->position, 4);
    reader->position += 4;
    return value;
}

float BinaryReader_readFloat32(BinaryReader* reader) {
    boundsCheck(reader, 4);
    float value;
    memcpy(&value, reader->buffer + reader->position, 4);
    reader->position += 4;
    return value;
}

uint64_t BinaryReader_readUint64(BinaryReader* reader) {
    boundsCheck(reader, 8);
    uint64_t value;
    memcpy(&value, reader->buffer + reader->position, 8);
    reader->position += 8;
    return value;
}

bool BinaryReader_readBool32(BinaryReader* reader) {
    return BinaryReader_readUint32(reader) != 0;
}

const char* BinaryReader_readStringPtr(BinaryReader* reader) {
    uint32_t offset = BinaryReader_readUint32(reader);
    if (offset == 0) return nullptr;
    if (offset >= reader->size) {
        fprintf(stderr, "BinaryReader: string pointer 0x%08X out of bounds (buffer size 0x%zX)\n", offset, reader->size);
        exit(1);
    }
    return (const char*)(reader->buffer + offset);
}

void BinaryReader_readBytes(BinaryReader* reader, void* dest, size_t count) {
    boundsCheck(reader, count);
    memcpy(dest, reader->buffer + reader->position, count);
    reader->position += count;
}

void BinaryReader_skip(BinaryReader* reader, size_t bytes) {
    boundsCheck(reader, bytes);
    reader->position += bytes;
}

void BinaryReader_seek(BinaryReader* reader, size_t position) {
    if (position > reader->size) {
        fprintf(stderr, "BinaryReader: seek to 0x%zX out of bounds (buffer size 0x%zX)\n", position, reader->size);
        exit(1);
    }
    reader->position = position;
}

size_t BinaryReader_getPosition(BinaryReader* reader) {
    return reader->position;
}
