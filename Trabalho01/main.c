#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 100
#define INSTRUCTION_COUNT 11

typedef struct {
    char mnemonic[5];
    uint8_t opcode;
    uint8_t funct3;
    uint8_t funct7;
    char type;
} Instruction;

typedef struct {
    uint32_t binary;
    char original[50];
} AssembledInstruction;

const Instruction INSTRUCTION_SET[INSTRUCTION_COUNT] = {
    // 7 instruções obrigatórias do grupo 14
    {"lh",  0x03, 0x1, 0x00, 'I'},
    {"sh",  0x23, 0x1, 0x00, 'S'},
    {"sub", 0x33, 0x0, 0x20, 'R'},
    {"or",  0x33, 0x6, 0x00, 'R'},
    {"andi",0x13, 0x7, 0x00, 'I'},
    {"srl", 0x33, 0x5, 0x00, 'R'},
    {"beq", 0x63, 0x0, 0x00, 'B'},
    // Instruções auxiliares obrigatórias
    {"add", 0x33, 0x0, 0x00, 'R'},
    {"addi",0x13, 0x0, 0x00, 'I'},
    {"sll", 0x33, 0x1, 0x00, 'R'},
    {"xor", 0x33, 0x4, 0x00, 'R'}
};

uint32_t parse_immediate(const char *str) {
    if (strncmp(str, "0x", 2) == 0) return strtoul(str, NULL, 16);
    if (strncmp(str, "0b", 2) == 0) return strtoul(str+2, NULL, 2);
    return strtoul(str, NULL, 10);
}

uint8_t register_number(const char *reg) {
    return (uint8_t)strtoul(reg+1, NULL, 10);
}

void expand_pseudo_instructions(char *line) {
    if (strncmp(line, "mv ", 3) == 0) {
        char rd[5], rs[5];
        sscanf(line, "mv %4s, %4s", rd, rs);
        sprintf(line, "addi %s, %s, 0", rd, rs);
    }
    else if (strncmp(line, "li ", 3) == 0) {
        char rd[5], imm[20];
        sscanf(line, "li %4s, %19s", rd, imm);
        sprintf(line, "addi %s, x0, %s", rd, imm);
    }
    else if (strncmp(line, "nop", 3) == 0) {
        strcpy(line, "addi x0, x0, 0");
    }
}

uint32_t encode_r_type(const Instruction *inst, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return (inst->funct7 << 25) | (rs2 << 20) | (rs1 << 15) 
         | (inst->funct3 << 12) | (rd << 7) | inst->opcode;
}

uint32_t encode_i_type(const Instruction *inst, uint8_t rd, uint8_t rs1, uint32_t imm) {
    return (imm << 20) | (rs1 << 15) | (inst->funct3 << 12) 
         | (rd << 7) | inst->opcode;
}

uint32_t encode_s_type(const Instruction *inst, uint8_t rs1, uint8_t rs2, uint32_t imm) {
    uint32_t imm_11_5 = (imm >> 5) & 0x7F;
    uint32_t imm_4_0 = imm & 0x1F;
    return (imm_11_5 << 25) | (rs2 << 20) | (rs1 << 15) | (inst->funct3 << 12) 
         | (imm_4_0 << 7) | inst->opcode;
}

uint32_t encode_b_type(const Instruction *inst, uint8_t rs1, uint8_t rs2, uint32_t imm) {
    // Imediato em B-type é dividido: imm[12|10:5|4:1|11]
    uint32_t imm_12 = (imm >> 12) & 0x1;
    uint32_t imm_11 = (imm >> 11) & 0x1;
    uint32_t imm_10_5 = (imm >> 5) & 0x3F;
    uint32_t imm_4_1 = (imm >> 1) & 0xF;
    
    return (imm_12 << 31) | (imm_10_5 << 25) | (rs2 << 20) | (rs1 << 15)
         | (inst->funct3 << 12) | (imm_4_1 << 8) | (imm_11 << 7) | inst->opcode;
}

uint32_t assemble_instruction(const char *line) {
    char mnemonic[5], rd[5], rs1[5], rs2[5], imm[20];
    const Instruction *inst = NULL;
    
    for (int i = 0; i < INSTRUCTION_COUNT; i++) {
        if (strncmp(line, INSTRUCTION_SET[i].mnemonic, strlen(INSTRUCTION_SET[i].mnemonic)) == 0) {
            inst = &INSTRUCTION_SET[i];
            break;
        }
    }
    
    if (!inst) return 0;

    switch (inst->type) {        case 'R': {
            sscanf(line, "%4s %4s, %4s, %4s", mnemonic, rd, rs1, rs2);
            return encode_r_type(inst, register_number(rd), 
                               register_number(rs1), register_number(rs2));
        }
        case 'I': {
            sscanf(line, "%4s %4s, %4s, %19s", mnemonic, rd, rs1, imm);
            return encode_i_type(inst, register_number(rd),
                               register_number(rs1), parse_immediate(imm));
        }        case 'S': {
            char offset[20], base[5];
            if (sscanf(line, "%4s %4s, %19[^(](%4[^)])", mnemonic, rs2, offset, base) == 4) {
                return encode_s_type(inst, register_number(base),
                                   register_number(rs2), parse_immediate(offset));
            } else {
                // Fallback para formato sem parênteses
                sscanf(line, "%4s %4s, %19s, %4s", mnemonic, rs2, imm, rs1);
                return encode_s_type(inst, register_number(rs1),
                                   register_number(rs2), parse_immediate(imm));
            }
        }
        case 'B': {
            sscanf(line, "%4s %4s, %4s, %19s", mnemonic, rs1, rs2, imm);
            return encode_b_type(inst, register_number(rs1),
                               register_number(rs2), parse_immediate(imm));
        }
        default: return 0;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s arquivo.asm [-o saida.txt]\n", argv[0]);
        return 1;
    }

    FILE *input = fopen(argv[1], "r");
    if (!input) {
        perror("Erro ao abrir arquivo de entrada");
        return 1;
    }

    AssembledInstruction *output = malloc(100 * sizeof(AssembledInstruction));
    int count = 0;
    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), input)) {
        if (line[0] == '\n' || line[0] == '#') continue;
        
        line[strcspn(line, "\n")] = 0;
        strcpy(output[count].original, line);
        
        expand_pseudo_instructions(line);
        output[count].binary = assemble_instruction(line);
        
        count++;
    }

    fclose(input);

    if (argc == 4 && strcmp(argv[2], "-o") == 0) {
        FILE *out = fopen(argv[3], "w");
        for (int i = 0; i < count; i++) {
            for (int b = 31; b >= 0; b--) {
                fprintf(out, "%d", (output[i].binary >> b) & 1);
            }
            fprintf(out, "\n");
        }
        fclose(out);
    } else {
        for (int i = 0; i < count; i++) {
            for (int b = 31; b >= 0; b--) {
                printf("%d", (output[i].binary >> b) & 1);
            }
            printf("\n");
        }
    }

    free(output);
    return 0;
}
