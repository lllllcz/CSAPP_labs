#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y64asm.h"

line_t *line_head = NULL;
line_t *line_tail = NULL;
int lineno = 0;

typedef enum
{
    NO_ERR = 0,
    DELIM_ERR,
    DUP_SYMBOL_ERR,
    DEST_ERR,
    DIRECTIVE_ERR,
    IMM_ERR,
    MEM_ERR,
    REG_ERR,
} err_t;
err_t errType = NO_ERR;
char *err_message = NULL;

#define err_print(_s, _a...)            \
    do                                  \
    {                                   \
        if (lineno < 0)                 \
            fprintf(stderr, "[--]: "_s  \
                            "\n",       \
                    ##_a);              \
        else                            \
            fprintf(stderr, "[L%d]: "_s \
                            "\n",       \
                    lineno, ##_a);      \
    } while (0);

int64_t vmaddr = 0; /* vm addr */

/* register table */
const reg_t reg_table[REG_NONE] = {
    {"%rax", REG_RAX, 4},
    {"%rcx", REG_RCX, 4},
    {"%rdx", REG_RDX, 4},
    {"%rbx", REG_RBX, 4},
    {"%rsp", REG_RSP, 4},
    {"%rbp", REG_RBP, 4},
    {"%rsi", REG_RSI, 4},
    {"%rdi", REG_RDI, 4},
    {"%r8", REG_R8, 3},
    {"%r9", REG_R9, 3},
    {"%r10", REG_R10, 4},
    {"%r11", REG_R11, 4},
    {"%r12", REG_R12, 4},
    {"%r13", REG_R13, 4},
    {"%r14", REG_R14, 4}};
const reg_t *find_register(char *name)
{
    int i;
    for (i = 0; i < REG_NONE; i++)
        if (!strncmp(name, reg_table[i].name, reg_table[i].namelen))
            return &reg_table[i];
    return NULL;
}

/* instruction set */
instr_t instr_set[] = {
    {"nop", 3, HPACK(I_NOP, F_NONE), 1},
    {"halt", 4, HPACK(I_HALT, F_NONE), 1},
    {"rrmovq", 6, HPACK(I_RRMOVQ, F_NONE), 2},
    {"cmovle", 6, HPACK(I_RRMOVQ, C_LE), 2},
    {"cmovl", 5, HPACK(I_RRMOVQ, C_L), 2},
    {"cmove", 5, HPACK(I_RRMOVQ, C_E), 2},
    {"cmovne", 6, HPACK(I_RRMOVQ, C_NE), 2},
    {"cmovge", 6, HPACK(I_RRMOVQ, C_GE), 2},
    {"cmovg", 5, HPACK(I_RRMOVQ, C_G), 2},
    {"irmovq", 6, HPACK(I_IRMOVQ, F_NONE), 10},
    {"rmmovq", 6, HPACK(I_RMMOVQ, F_NONE), 10},
    {"mrmovq", 6, HPACK(I_MRMOVQ, F_NONE), 10},
    {"addq", 4, HPACK(I_ALU, A_ADD), 2},
    {"subq", 4, HPACK(I_ALU, A_SUB), 2},
    {"andq", 4, HPACK(I_ALU, A_AND), 2},
    {"xorq", 4, HPACK(I_ALU, A_XOR), 2},
    {"jmp", 3, HPACK(I_JMP, C_YES), 9},
    {"jle", 3, HPACK(I_JMP, C_LE), 9},
    {"jl", 2, HPACK(I_JMP, C_L), 9},
    {"je", 2, HPACK(I_JMP, C_E), 9},
    {"jne", 3, HPACK(I_JMP, C_NE), 9},
    {"jge", 3, HPACK(I_JMP, C_GE), 9},
    {"jg", 2, HPACK(I_JMP, C_G), 9},
    {"call", 4, HPACK(I_CALL, F_NONE), 9},
    {"ret", 3, HPACK(I_RET, F_NONE), 1},
    {"pushq", 5, HPACK(I_PUSHQ, F_NONE), 2},
    {"popq", 4, HPACK(I_POPQ, F_NONE), 2},
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1},
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2},
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4},
    {".quad", 5, HPACK(I_DIRECTIVE, D_DATA), 8},
    {".pos", 4, HPACK(I_DIRECTIVE, D_POS), 0},
    {".align", 6, HPACK(I_DIRECTIVE, D_ALIGN), 0},
    {NULL, 1, 0, 0} // end
};

instr_t *find_instr(char *name)
{
    int i;
    for (i = 0; instr_set[i].name; i++)
        if (strncmp(instr_set[i].name, name, instr_set[i].len) == 0)
            return &instr_set[i];
    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{
    symbol_t *tmp = NULL;
    tmp = symtab->next;
    while (tmp)
    {
        if (strcmp(tmp->name, name) == 0)
            return tmp;
        tmp = tmp->next;
    }

    return NULL;
}

/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)
{
    /* check duplicate */
    if (find_symbol(name) != NULL)
        return -1;

    /* create new symbol_t (don't forget to free it)*/
    symbol_t *symbol = (symbol_t *)malloc(sizeof(symbol_t));
    symbol->name = (char *)malloc(MAX_INSLEN);
    strcpy(symbol->name, name);
    symbol->addr = vmaddr;

    /* add the new symbol_t to symbol table */
    symbol->next = symtab->next;
    symtab->next = symbol;

    return 0;
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 */
void add_reloc(char *name, bin_t *bin)
{
    /* create new reloc_t (don't forget to free it)*/
    reloc_t *reloc = (reloc_t *)malloc(sizeof(reloc_t));
    reloc->name = (char *)malloc(MAX_INSLEN);
    strcpy(reloc->name, name);
    reloc->y64bin = bin;

    /* add the new reloc_t to relocation table */
    reloc->next = reltab->next;
    reltab->next = reloc;

    return;
}

/* macro for parsing y64 assembly code */
#define IS_DIGIT(s) ((*(s) >= '0' && *(s) <= '9') || *(s) == '-' || *(s) == '+')
#define IS_LETTER(s) ((*(s) >= 'a' && *(s) <= 'z') || (*(s) >= 'A' && *(s) <= 'Z'))
#define IS_COMMENT(s) (*(s) == '#')
#define IS_REG(s) (*(s) == '%')
#define IS_IMM(s) (*(s) == '$')

#define IS_BLANK(s) (*(s) == ' ' || *(s) == '\t')
#define IS_END(s) (*(s) == '\0')

#define SKIP_BLANK(s)                     \
    do                                    \
    {                                     \
        while (!IS_END(s) && IS_BLANK(s)) \
            (s)++;                        \
    } while (0);

/* return value from different parse_xxx function */
typedef enum
{
    PARSE_ERR = -1,
    PARSE_REG,
    PARSE_DIGIT,
    PARSE_SYMBOL,
    PARSE_MEM,
    PARSE_DELIM,
    PARSE_INSTR,
    PARSE_LABEL
} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovq')
 * args
 *     ptr: point to the start of string
 *     inst: point to the inst_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
    if (*ptr == NULL)
        return PARSE_ERR;
    /* skip the blank */
    SKIP_BLANK(*ptr);
    if (IS_END(*ptr))
        return PARSE_ERR;

    /* find_instr and check end */
    (*inst) = find_instr(*ptr);

    /* set 'ptr' and 'inst' */
    if ((*inst) == NULL)
    {
        // printf("null null null null null null\n");
        // printf("the string: %s\n", *ptr);
        return PARSE_ERR;
    }

    for (int i = 0; i < (*inst)->len; i++)
        (*ptr) += 1;
    SKIP_BLANK(*ptr);
    // printf("no empty?\n");
    // printf("the string: %s\n", *ptr);
    return PARSE_INSTR;
}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    if (IS_END(*ptr) || ptr == NULL)
        return PARSE_ERR;

    /* set 'ptr' */
    // err_print("%s???", *ptr);
    if (**ptr == delim)
    {
        (*ptr) += 1;
        return PARSE_DELIM;
    }

    // printf("null delim null delim null delim\n");
    // printf("the string: %s\n", *ptr);
    errType = DELIM_ERR;
    // err_print("%s???", *ptr);
    return PARSE_ERR;
}

/*
 * parse_reg: parse an expected register token (e.g., '%rax')
 * args
 *     ptr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token,
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);

    /* find register */
    const reg_t *myReg = find_register(*ptr);

    /* set 'ptr' and 'regid' */
    if (myReg == NULL)
    {
        // if there is no register, must be wrong
        // printf("null reg null reg null reg\n");
        // printf("the string: %s\n", *ptr);
        errType = REG_ERR;
        return PARSE_ERR;
    }

    for (int i = 0; i < myReg->namelen; i++)
        (*ptr) += 1;
    // printf("the string: %s\n", *ptr);
    *regid = myReg->id;
    return PARSE_REG;
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    if (ptr == NULL || IS_END(*ptr))
        return PARSE_ERR;

    char *symbolName = *ptr;
    int num = 0;
    symbol_t *sym = NULL;

    while (IS_LETTER(symbolName) || IS_DIGIT(symbolName))
    {
        ++num;
        symbolName += 1;
    }

    /* allocate name and copy to it */
    symbolName = NULL;
    symbolName = (char *)malloc(num + 1);
    strncpy(symbolName, *ptr, num);
    *(symbolName + num) = '\0';

    sym = find_symbol(symbolName);
    (*ptr) += num;
    if (sym == NULL)
    {
        // not found
        // need relocate
        *name = symbolName;
        errType = DIRECTIVE_ERR;
        return PARSE_ERR;
    }

    /* set 'ptr' and 'name' */
    *name = sym->name;
    free(symbolName);
    // printf("name: %s\n", *name);

    return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *value)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    if (IS_END(*ptr) || ptr == NULL)
        return PARSE_ERR;
    if (**ptr == '$')
        (*ptr) += 1;
    char *istr = *ptr;
    bool_t negative = FALSE;

    /* calculate the digit, (NOTE: see strtoll()) */
    if (*istr == '-')
    {
        negative = TRUE;
        istr += 1;
        (*ptr) += 1;
    }
    if (*istr == '0' && *(istr + 1) == 'x')
    {
        /* set 'ptr' and 'value' */
        // hex
        // printf("hex\t");
        (*ptr) += 2;
        while (((**ptr) >= '0' && (**ptr) <= '9') || ((**ptr) >= 'a' && (**ptr) <= 'f') || ((**ptr) >= 'A' && (**ptr) <= 'F'))
        {
            (*ptr) += 1;
        }
        *value = strtoull(istr, ptr, 16);
        // printf("value: %lx, string: %s\n", *value, istr);
    }
    else
    {
        /* set 'ptr' and 'value' */
        // dec
        // printf("dec\t");
        if (**ptr >= 'a' && **ptr <= 'z')
        {
            errType = IMM_ERR;
            return PARSE_ERR;
        }
        while ((**ptr) >= '0' && (**ptr) <= '9')
        {
            (*ptr) += 1;
        }
        *value = strtoll(istr, ptr, 10);
        // printf("value: %ld, string: %s\n", *value, istr);
    }

    if (negative == TRUE)
        *value *= (-1);

    if (value == NULL)
    {
        // printf("null value null value null value\n");
        return PARSE_ERR;
    }
    else
    {
        return PARSE_DIGIT;
    }
}

/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *value)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    if (IS_END(*ptr) || ptr == NULL)
        return PARSE_ERR;

    if (IS_IMM(*ptr))
    {
        /* if IS_IMM, then parse the digit */
        return parse_digit(ptr, value);
    }
    else if (IS_LETTER(*ptr))
    {
        /* if IS_LETTER, then parse the symbol */
        if (parse_symbol(ptr, name) == PARSE_ERR)
        {
            if (errType != DIRECTIVE_ERR)
                errType = DEST_ERR;
            return PARSE_ERR;
        }
        symbol_t *sym = find_symbol(*name);
        *value = sym->addr;
        return PARSE_SYMBOL;
    }
    else
    {
        // printf("no imm; no letter\n");
        errType = IMM_ERR;
        return PARSE_ERR;
    }

    /* set 'ptr' and 'name' or 'value' */
}

/*
 * parse_mem: parse an expected memory token (e.g., '8(%rbp)')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_MEM: success, move 'ptr' to the first char after token,
 *                          and store the value of digit to 'value',
 *                          and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr', 'value' and 'regid' are undefined
 */
parse_t parse_mem(char **ptr, long *value, regid_t *regid)
{
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    if (IS_END(*ptr) || ptr == NULL)
        return PARSE_ERR;

    /* calculate the digit and register, (ex: (%rbp) or 8(%rbp)) */
    /* set 'ptr', 'value' and 'regid' */
    if ((**ptr) == '-' || ((**ptr) >= '0' && (**ptr) <= '9'))
    {
        if (parse_digit(ptr, value) == PARSE_ERR)
        {
            errType = MEM_ERR;
            return PARSE_ERR;
        }
    }
    if ((**ptr) != '(')
    {
        errType = MEM_ERR;
        return PARSE_ERR;
    }
    (*ptr) += 1;
    if (parse_reg(ptr, regid) == PARSE_ERR)
    {
        return PARSE_ERR;
    }
    if ((**ptr) != ')')
    {
        errType = MEM_ERR;
        return PARSE_ERR;
    }
    (*ptr) += 1;

    return PARSE_MEM;
}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *value)
{
    // I use this function to parse the jmp/call target
    /* skip the blank and check */
    bool_t isNegative = FALSE;
    SKIP_BLANK(*ptr);
    if (IS_END(*ptr) || ptr == NULL)
    {
        errType = DEST_ERR;
        return PARSE_ERR;
    }

    if (**ptr == '-') {
        isNegative = TRUE;
        *ptr += 1;
    }

    if (IS_DIGIT(*ptr))
    {
        /* if IS_DIGIT, then parse the digit */
        if (**ptr != '0' && *(*ptr + 1) != 'x')
        {
            errType = DEST_ERR;
            return PARSE_ERR;
        }

        if (parse_digit(ptr, value) == PARSE_ERR) {
            return PARSE_ERR;
        }

        if (isNegative == TRUE) {
            *value *= (-1);
        }
        return PARSE_DIGIT;
    }
    else if (IS_LETTER(*ptr))
    {
        /* if IS_LETTER, then parse the symbol */
        if (parse_symbol(ptr, name) == PARSE_ERR)
        {
            if (errType != DIRECTIVE_ERR)
            {
                errType = DEST_ERR;
            }
            return PARSE_ERR;
        }
        symbol_t *sym = find_symbol(*name);
        *value = sym->addr;
        return PARSE_SYMBOL;
    }
    else
    {
        errType = DEST_ERR;
        return PARSE_ERR;
    }

    /* set 'ptr', 'name' and 'value' */
    // no need
}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name)
{
    // this function can make 'name == null', and return label or symbol
    /* skip the blank and check */
    SKIP_BLANK(*ptr);
    if (IS_END(*ptr) || ptr == NULL)
        return PARSE_ERR;

    char *label = *ptr;
    int num = 0;

    while (*label != ':')
    {
        ++num;
        label += 1;
        if (IS_END(label))
        {
            return PARSE_LABEL;
        }
    }

    /* allocate name and copy to it */
    char *labelName = (char *)malloc(num + 1);
    strncpy(labelName, *ptr, num);
    *(labelName + num) = '\0';

    if (find_symbol(labelName))
    {
        errType = DUP_SYMBOL_ERR;
        err_message = labelName;
        return PARSE_ERR;
    }

    add_symbol(labelName);
    free(labelName);

    /* set 'ptr' and 'name' */
    label += 1;
    SKIP_BLANK(label);
    *ptr = label;

    return PARSE_LABEL;
}

/*
 * parse_line: parse a line of y64 code (e.g., 'Loop: mrmovq (%rcx), %rsi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y64 assembly code
 *
 * return
 *     TYPE_XXX: success, fill line_t with assembled y64 code
 *     TYPE_ERR: error, try to print err information
 */
type_t parse_line(line_t *line)
{

    /* when finish parse an instruction or lable, we still need to continue check
     * e.g.,
     *  Loop: mrmovl (%rbp), %rcx
     *           call SUM  #invoke SUM function */

    char *istr = NULL;
    istr = line->y64asm;
    char *labelName = NULL;
    instr_t *ins = NULL;
    regid_t regA = REG_NONE, regB = REG_NONE;
    char *symbolName = NULL;
    long val = 0;

    /* skip blank and check IS_END */
    SKIP_BLANK(istr);
    if (IS_END(istr))
    {
        line->type = TYPE_COMM;
        return TYPE_COMM;
    }

    /* is a comment ? */
    if (IS_COMMENT(istr))
    {
        line->type = TYPE_COMM;
        return line->type;
    }

    /* is a label ? */
    if (parse_label(&istr, &labelName) == PARSE_ERR)
    {
        // 'parse_label' move the pointer to the end of label
        //      and put its name in 'labelName'
        // or keep pointer the same (skip blanks), labelName == null
        line->type = TYPE_ERR;
        return TYPE_ERR;
    }
    SKIP_BLANK(istr);
    if (IS_END(istr) || IS_COMMENT(istr))
    {
        line->type = TYPE_COMM;
        return TYPE_COMM;
    }

    /* is an instruction ? */
    if (parse_instr(&istr, &ins) == PARSE_ERR)
    {
        line->type = TYPE_ERR;
        return TYPE_ERR;
    }
    // now, the pointer at the end of instruction
    // attention! the pointer can't be the same place!!!
    // anyway, after the pointer is a comment or the end

    /* set type and y64bin */
    line->type = TYPE_INS;
    line->y64bin.addr = vmaddr;
    line->y64bin.codes[0] = ins->code;
    line->y64bin.bytes = ins->bytes;

    /* update vmaddr */
    vmaddr += ins->bytes;

    /* parse the rest of instruction according to the itype */
    // don't need to parse the comment after the instruction
    itype_t icode = HIGH(ins->code);
    switch (icode)
    {
    case I_HALT:
    case I_NOP:
        break;
    case I_POPQ:
    case I_PUSHQ:
        if (parse_reg(&istr, &regA) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        line->y64bin.codes[1] = HPACK(regA, regB);
        break;
    case I_RRMOVQ:
    case I_ALU:
        // read two registers
        if (parse_reg(&istr, &regA) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        if (parse_delim(&istr, ',') == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        if (parse_reg(&istr, &regB) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        line->y64bin.codes[1] = HPACK(regA, regB);
        // printf("%x %x\n", line->y64bin.codes[0], line->y64bin.codes[1]);
        break;
    case I_JMP:
    case I_CALL:
        // read the address
        if (parse_data(&istr, &symbolName, &val) == PARSE_ERR)
        {
            if (errType == DIRECTIVE_ERR)
            {
                add_reloc(symbolName, &line->y64bin);
                free(symbolName);
                break;
            }
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        for (int i = 0; i < 8; ++i)
            line->y64bin.codes[i + 1] = (val >> i * 8) & 0xff;
        break;
    case I_IRMOVQ:
        if (parse_imm(&istr, &symbolName, &val) == PARSE_ERR)
        {
            if (errType == DIRECTIVE_ERR)
            {
                add_reloc(symbolName, &line->y64bin);
                free(symbolName);
                if (parse_delim(&istr, ',') == PARSE_ERR)
                {
                    line->type = TYPE_ERR;
                    return TYPE_ERR;
                }
                if (parse_reg(&istr, &regB) == PARSE_ERR)
                {
                    line->type = TYPE_ERR;
                    return TYPE_ERR;
                }
                line->y64bin.codes[1] = HPACK(regA, regB);
                break;
            }
            errType = IMM_ERR;
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        if (parse_delim(&istr, ',') == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        if (parse_reg(&istr, &regB) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        line->y64bin.codes[1] = HPACK(regA, regB);
        for (int i = 0; i < 8; ++i)
            line->y64bin.codes[i + 2] = (val >> i * 8) & 0xff;
        break;
    case I_MRMOVQ:
        if (parse_mem(&istr, &val, &regA) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        if (parse_delim(&istr, ',') == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        if (parse_reg(&istr, &regB) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        line->y64bin.codes[1] = HPACK(regB, regA);
        for (int i = 0; i < 8; ++i)
            line->y64bin.codes[i + 2] = (val >> i * 8) & 0xff;
        break;
    case I_RMMOVQ:
        if (parse_reg(&istr, &regA) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        if (parse_delim(&istr, ',') == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        if (parse_mem(&istr, &val, &regB) == PARSE_ERR)
        {
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        line->y64bin.codes[1] = HPACK(regA, regB);
        // printf("value: %ld\n", val);
        for (int i = 0; i < 8; ++i)
            line->y64bin.codes[i + 2] = (val >> i * 8) & 0xff;
        break;
    default:
        break;
    }

    if (icode == I_DIRECTIVE && ins->bytes != 0)
    {
        if (parse_data(&istr, &symbolName, &val) == PARSE_ERR)
        {
            if (errType == DIRECTIVE_ERR)
            {
                add_reloc(symbolName, &line->y64bin);
                free(symbolName);
                return line->type;
            }
            errType = DIRECTIVE_ERR;
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        switch (ins->bytes)
        {
        case 1:
            // .byte
            line->y64bin.codes[0] = val;
            break;
        case 2:
            // .word
            for (int i = 0; i < 2; ++i)
                line->y64bin.codes[i] = (val >> i * 8) & 0xff;
            break;
        case 4:
            // .long
            for (int i = 0; i < 4; ++i)
                line->y64bin.codes[i] = (val >> i * 8) & 0xff;
            break;
        case 8:
            // .quad
            for (int i = 0; i < 8; ++i)
                line->y64bin.codes[i] = (val >> i * 8) & 0xff;
            break;
        default:
            break;
        }
    }

    if (icode == I_DIRECTIVE && ins->bytes == 0)
    {
        if (parse_digit(&istr, &val) == PARSE_ERR)
        {
            errType = DIRECTIVE_ERR;
            line->type = TYPE_ERR;
            return TYPE_ERR;
        }
        if (ins->len == 4)
        {
            // .pos
            vmaddr = val;
        }
        else
        {
            // .align
            while (vmaddr % val != 0)
            {
                vmaddr++;
            }
        }
    }

    return line->type;
}

/*
 * assemble: assemble an y64 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y64 assembly file)
 *
 * return
 *     0: success, assmble the y64 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y64asm;

    /* read y64 code line-by-line, and parse them to generate raw y64 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL)
    {
        slen = strlen(asm_buf);
        while ((asm_buf[slen - 1] == '\n') || (asm_buf[slen - 1] == '\r'))
        {
            asm_buf[--slen] = '\0'; /* replace terminator */
        }

        /* store y64 assembly code */
        y64asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y64asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        line->type = TYPE_COMM;
        line->y64asm = y64asm;
        line->next = NULL;

        line_tail->next = line;
        line_tail = line;
        lineno++;

        if (parse_line(line) == TYPE_ERR)
        {
            switch (errType)
            {
            case NO_ERR:
                break;
            case DELIM_ERR:
                err_print("Invalid ','");
                break;
            case DUP_SYMBOL_ERR:
                err_print("Dup symbol:%s", err_message);
                free(err_message);
                break;
            case DEST_ERR:
                err_print("Invalid DEST");
                break;
            case DIRECTIVE_ERR:
                break;
            case IMM_ERR:
                err_print("Invalid Immediate");
                break;
            case MEM_ERR:
                err_print("Invalid MEM");
                break;
            case REG_ERR:
                err_print("Invalid REG");
                break;
            default:
                break;
            }
            return -1;
        }
    }

    lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y64 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
    reloc_t *rtmp = NULL;
    char *name = NULL;
    symbol_t *sym = NULL;
    int num = 1;

    rtmp = reltab->next;
    while (rtmp)
    {
        /* find symbol */
        name = rtmp->name;
        sym = find_symbol(name);
        if (sym == NULL)
        {
            err_print("Unknown symbol:'%s'", name);
            return -1;
        }

        /* relocate y64bin according itype */
        if (rtmp->y64bin->codes[0] == HPACK(I_IRMOVQ, F_NONE))
        {
            num = 2;
        }
        if (rtmp->y64bin->codes[0] == HPACK(I_DIRECTIVE, D_DATA))
        {
            num = 0;
        }
        for (int i = 0; i < 8; ++i)
            rtmp->y64bin->codes[i + num] = (sym->addr >> i * 8) & 0xff;

        /* next */
        rtmp = rtmp->next;
    }
    return 0;
}

/*
 * binfile: generate the y64 binary file
 * args
 *     out: point to output file (an y64 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
    /* prepare image with y64 binary code */
    /* binary write y64 code to output file (NOTE: see fwrite()) */
    line_t *line = NULL;
    line = line_head->next;
    char codes[MAX_INSLEN];
    long addr = 0;
    int bytes;
    for (int i = 0; i < MAX_INSLEN; i++)
        codes[i] = 0;

    while (line)
    {
        bytes = line->y64bin.bytes;
        if (bytes != 0)
        {
            if (line->y64bin.addr > addr)
            {
                if (fwrite(codes, 1, line->y64bin.addr - addr, out) != line->y64bin.addr - addr)
                    return -1;
            }
            if (line->type == TYPE_INS)
            {
                if (fwrite(line->y64bin.codes, 1, bytes, out) != bytes)
                    return -1;
            }

            addr = line->y64bin.addr + line->y64bin.bytes;
        }
        line = line->next;
    }

    return 0;
}

/* whether print the readable output to screen or not ? */
bool_t screen = FALSE;

static void hexstuff(char *dest, int value, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        char c;
        int h = (value >> 4 * i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len - i - 1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[64];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS)
    {
        bin_t *y64bin = &line->y64bin;
        int i;

        strcpy(buf, "  0x000:                      | ");

        hexstuff(buf + 4, y64bin->addr, 3);
        if (y64bin->bytes > 0)
            for (i = 0; i < y64bin->bytes; i++)
                hexstuff(buf + 9 + 2 * i, y64bin->codes[i] & 0xFF, 2);
    }
    else
    {
        strcpy(buf, "                              | ");
    }

    printf("%s%s\n", buf, line->y64asm);
}

/*
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = line_head->next;
    while (tmp != NULL)
    {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    line_head = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(line_head, 0, sizeof(line_t));
    line_tail = line_head;
    lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do
    {
        rtmp = reltab->next;
        if (reltab->name)
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);

    symbol_t *stmp = NULL;
    do
    {
        stmp = symtab->next;
        if (symtab->name)
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do
    {
        ltmp = line_head->next;
        if (line_head->y64asm)
            free(line_head->y64asm);
        free(line_head);
        line_head = ltmp;
    } while (line_head);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;

    if (argc < 2)
        usage(argv[0]);

    if (argv[nextarg][0] == '-')
    {
        char flag = argv[nextarg][1];
        switch (flag)
        {
        case 'v':
            screen = TRUE;
            nextarg++;
            break;
        default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg]) - 3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg] + rootlen, ".ys"))
        usage(argv[0]);

    if (rootlen > 500)
    {
        err_print("File name too long");
        exit(1);
    }

    /* init */
    init();

    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname + rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in)
    {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }

    if (assemble(in) < 0)
    {
        err_print("Assemble y64 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);

    /* relocate binary code */
    if (relocate() < 0)
    {
        err_print("Relocate binary code error");
        exit(1);
    }

    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname + rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out)
    {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0)
    {
        err_print("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);

    /* print to screen (.yo file) */
    if (screen)
        print_screen();

    /* finit */
    finit();
    return 0;
}
