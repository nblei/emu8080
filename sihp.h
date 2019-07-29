// Simple Intel Hex Parser --- only usefule for our emulator
#ifndef __SIHP_H__
#define __SIHP_H__
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct record {
    uint8_t byte_count;
    uint16_t address;
    enum {
        data = 00,
        eof = 01,
        es_addr = 02, // extend segment
        ss_addr = 03, // start segment --- initial program counter / instr pointer
        ex_lin_addr = 04, // extended linear address --- for 32 bit addresses
        s_lin_addr = 05 // start linear addresss --- for 32 bit addresses
    } record_type;
    uint8_t data[256];
    uint8_t checksum;
};


int
nibble2int(char nibble)
{
    switch (nibble) {
        case '0' ... '9':
            return nibble - '0';
        case 'a' ... 'f':
            return nibble - 'a' + 10;
        case 'A' ... 'F':
            return nibble - 'A' + 10;
        default:
            fprintf(stderr, "Illegal character %d '%c' in 'nibble2int'",
                    nibble, nibble);
            exit(1);
    }
}


uint8_t
byte2int(char high, char low)
{
    return (nibble2int(high) << 4) | nibble2int(low);
}


uint16_t
word2int(char highest, char high, char low, char lowest)
{
    return (nibble2int(highest) << 12) |
           (nibble2int(high) << 8) |
           (nibble2int(low) << 4) |
           nibble2int(lowest);
}


void
_parse_line(struct record * restrict rec, const char * restrict line)
{
    int checksum_calc = 0;
    uint8_t csum;
    if (*line != ':') {
        fprintf(stderr, "Malformed line: %s\n\tLine does not begin with ':'\n",
                line);
        exit(1);
    }

    rec->byte_count = byte2int(line[1], line[2]);
    rec->address = word2int(line[3], line[4], line[5], line[6]);

    if (line[7] != '0') {
        fprintf(stderr, "Malformed line.  First nibble of record type is "
                "%d '%c'\n", line[7], line[7]);
        exit(1);

    }
    rec->record_type = nibble2int(line[8]);

    for (int i = 0; i < rec->byte_count; ++i) {
        rec->data[i] = byte2int(line[9 + 2*i], line[10 + 2*i]);
        checksum_calc += rec->data[i];
    }
    checksum_calc += rec->byte_count;
    checksum_calc += rec->address & 0xFF;
    checksum_calc += rec->address >> 8;
    checksum_calc += rec->record_type;
    checksum_calc &= 0xFF;
    checksum_calc = -checksum_calc;
    csum = (uint8_t)(unsigned)(checksum_calc & 0xFF);
    rec->checksum = byte2int(line[9 + 2*rec->byte_count], 
                             line[9 + 2*rec->byte_count + 1]);
    if (csum != rec->checksum) {
        fprintf(stderr, "Malformed line.  Checksums do not match; %d != %d\n",
                        csum, rec->checksum);
        exit(1);
    }
}


void
intel_hex_to_binary(FILE * hex, char ** mem, uint16_t *start_addr)
{
    char * linebuf = NULL;
    size_t N = 0;
    struct record record;
    uint16_t base_addr = 0;
    uint16_t addr = 0;
    *mem = calloc(1, 1 << 20);
    if (*mem == NULL) {
        perror("malloc");
        exit(1);
    }
    rewind(hex);
    *start_addr = 0;

    while (-1 != getline(&linebuf, &N, hex)) {
        _parse_line(&record, linebuf);
        switch (record.record_type) {
            case data:
                addr = record.address;
                memcpy(*mem + base_addr + addr,
                        &record.data, record.byte_count);
                addr += record.byte_count;
                break;
            case eof:
                goto parse_done;
            case es_addr:
                base_addr = 16 * (((uint16_t)(record.data[0] << 8)) |
                               (uint16_t)record.data[1]);
                break;
            case ss_addr:
                // Since the two MSBytes are CS, we ignore them
                *start_addr = ((uint16_t)(record.data[0] << 8)) |
                              (uint16_t)(record.data[1]);
                break;
            case ex_lin_addr:
            case s_lin_addr:
                fprintf(stderr, "Parser does not support Extended Linear "
                        "Address or Start Linear Address record fields");
                exit(1);
            default:
                fprintf(stderr, "Unexpected record type %d\n",
                        record.record_type);
                exit(1);
        }
    }
    if (ferror(hex)) {
        perror("getline");
        exit(1);
    }
    else {
        fprintf(stderr, "No EOF record found\n");
        exit(1);
    }
parse_done:
    return;
}



#endif
