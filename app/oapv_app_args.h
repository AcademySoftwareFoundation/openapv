/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the copyright owner, nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _OAPV_APP_ARGS_H_
#define _OAPV_APP_ARGS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "oapv.h"

#define ARGS_VAL_TYPE_MANDATORY      (1 << 0) /* mandatory or not */
#define ARGS_VAL_TYPE_NONE           (1 << 2) /* no value */
#define ARGS_VAL_TYPE_INTEGER        (2 << 2) /* integer type value */
#define ARGS_VAL_TYPE_STRING         (3 << 2) /* string type value */
#define ARGS_GET_CMD_OPT_VAL_TYPE(x) ((x) & 0x0C)
#define ARGS_GET_IS_OPT_TYPE_PPT(x)  (((x) >> 1) & 0x01)

#define ARGS_END_KEY                 (0)
#define ARGS_NO_KEY                  (127)
#define ARGS_KEY_LONG_CONFIG         "config"
#define ARGS_MAX_NUM_CONF_FILES      (16)

#define ARGS_MAX_KEY_LONG            (32)
#define ARGS_MAX_DESC                (1024) /* max length of an option description */
/* upper bound of the fixed prefix of a help line: key/key_long, value type,
 * "(optional)", default value string and surrounding decoration */
#define ARGS_HELP_PREFIX_MAX         (512)
/* buffer size for a formatted help line: description plus its prefix */
#define ARGS_HELP_BUF_SIZE           (ARGS_MAX_DESC + ARGS_HELP_PREFIX_MAX)

typedef struct args_opt {
    char  key;                         /* option keyword. ex) -f */
    char  key_long[ARGS_MAX_KEY_LONG]; /* option long keyword, ex) --file */
    int   val_type;                    /* value type */
    int   flag;                        /* flag to setting or not */
    void *val;                         /* actual value */
    int   val_size;                    /* size of value buffer for STRING type (0 for non-STRING) */
    char  desc[ARGS_MAX_DESC];          /* description of option */
} args_opt_t;

typedef struct args_parser args_parser_t;
struct args_parser {
    void (*release)(args_parser_t *args);
    int (*parse)(args_parser_t *args, int argc, const char *argv[], char **errstr);
    int (*get_help)(args_parser_t *args, int idx, char *help);
    int (*get_int)(args_parser_t *args, char *keyl, int *val, int *flag);
    int (*set_str)(args_parser_t *args, char *keyl, char *str);
    int (*set_int)(args_parser_t *args, char *keyl, int val);
    int (*set_int2str)(args_parser_t* args, char* keyl, int val);
    int (*set_flag)(args_parser_t *args, char *keyl, int flag);
    int (*check_mandatory)(args_parser_t *args, char **err_arg);

    args_opt_t *opts;
    int         num_option;
};

static int args_search_long_key(args_opt_t *opts, const char *key)
{
    args_opt_t *o;
    int         oidx = 0;

    o = opts;
    while(o->key != ARGS_END_KEY) {
        if(!strcmp(key, o->key_long)) {
            return oidx;
        }
        oidx++;
        o++;
    }
    return -1;
}

static int args_search_short_arg(args_opt_t *ops, const char key)
{
    args_opt_t *o;
    int         oidx = 0;

    o = ops;

    while(o->key != ARGS_END_KEY) {
        if(o->key != ARGS_NO_KEY && o->key == key) {
            return oidx;
        }
        oidx++;
        o++;
    }
    return -1;
}

static int args_read_value(args_opt_t *ops, const char *argv)
{
    if(argv == NULL || ops->val == NULL) {
        return -1;
    }
    if(argv[0] == '-' && (argv[1] < '0' || argv[1] > '9'))
        return -1;

    switch(ARGS_GET_CMD_OPT_VAL_TYPE(ops->val_type)) {
    case ARGS_VAL_TYPE_INTEGER:
        {
            char *endptr;
            long val;
            errno = 0;
            val = strtol(argv, &endptr, 10);
            if(endptr == argv || *endptr != '\0' || errno == ERANGE || val < INT_MIN || val > INT_MAX) {
                return -1;
            }
            *((int *)ops->val) = (int)val;
        }
        break;

    case ARGS_VAL_TYPE_STRING:
        if(ops->val_size > 0) {
            strncpy((char *)ops->val, argv, ops->val_size - 1);
            ((char *)ops->val)[ops->val_size - 1] = '\0';
        } else {
            return -1; /* STRING option without a known buffer size: reject */
        }
        break;

    default:
        return -1;
    }
    return 0;
}

static int args_parse_int_x_int(char *str, int *num0, int *num1)
{
    char  str0_t[64];
    int   i, cnt0 = 0, cnt1;
    char *str0, *str1 = NULL;
    char *endptr;
    long  tmp_val;

    str0 = str;
    cnt1 = (int)strlen(str);

    /* find 'x' */
    for(i = 0; i < (int)strlen(str); i++) {
        if(str[i] == 'x' || str[i] == 'X') {
            str1 = str + i + 1;
            cnt0 = i;
            cnt1 = cnt1 - cnt0 - 1;
            break;
        }
    }

    /* check malformed data */
    if(str1 == NULL || cnt0 == 0 || cnt1 == 0)
        return -1;

    for(i = 0; i < cnt0; i++) {
        if(str0[i] < 0x30 || str0[i] > 0x39)
            return -1; /* not a number */
    }
    for(i = 0; i < cnt1; i++) {
        if(str1[i] < 0x30 || str1[i] > 0x39)
            return -1; /* not a number */
    }

    strncpy(str0_t, str0, cnt0);
    str0_t[cnt0] = '\0';

    /* parse first number using strtol with overflow check */
    tmp_val = strtol(str0_t, &endptr, 10);
    if(endptr == str0_t || tmp_val < INT_MIN || tmp_val > INT_MAX) {
        return -1; /* conversion failed or overflow */
    }
    *num0 = (int)tmp_val;

    /* parse second number using strtol with overflow check */
    tmp_val = strtol(str1, &endptr, 10);
    if(endptr == str1 || tmp_val < INT_MIN || tmp_val > INT_MAX) {
        return -1; /* conversion failed or overflow */
    }
    *num1 = (int)tmp_val;

    return 0;
}

static int args_parse_cfg(FILE *fp, args_opt_t *ops, int is_type_ppt)
{
    char *parser;
    char  line[256] = "", tag[50] = "", val[256] = "";
    int   oidx;

    while(fgets(line, sizeof(line), fp)) {
        parser = strchr(line, '#');
        if(parser != NULL)
            *parser = '\0';

        parser = strtok(line, "= \t");
        if(parser == NULL)
            continue;
        strncpy(tag, parser, sizeof(tag) - 1);

        parser = strtok(NULL, "=\n");
        if(parser == NULL)
            continue;
        strncpy(val, parser, sizeof(val) - 1);

        oidx = args_search_long_key(ops, tag);
        if(oidx < 0)
            continue;

        if(ops[oidx].val == NULL) {
            return -1;
        }

        if(ARGS_GET_IS_OPT_TYPE_PPT(ops[oidx].val_type) == is_type_ppt) {
            if(ARGS_GET_CMD_OPT_VAL_TYPE(ops[oidx].val_type) != ARGS_VAL_TYPE_NONE) {
                if(args_read_value(ops + oidx, val))
                    continue;
            }
            else {
                *((int *)ops[oidx].val) = 1;
            }
            ops[oidx].flag = 1;
        }
    }
    return 0;
}

static int args_parse_cmd(int argc, const char *argv[], args_opt_t *ops,
                          int *idx, char **errstr)
{
    int aidx; /* arg index */
    int oidx; /* option index */

    aidx = *idx + 1;

    if(aidx >= argc || argv[aidx] == NULL)
        goto NO_MORE;
    if(argv[aidx][0] != '-')
        goto ERR;

    if(argv[aidx][1] == '-') {
        /* long option */
        oidx = args_search_long_key(ops, argv[aidx] + 2);
        if(oidx < 0) {
            *errstr = (char *)argv[aidx];
            goto ERR;
        }
    }
    else if(strlen(argv[aidx]) == 2) {
        /* short option */
        oidx = args_search_short_arg(ops, argv[aidx][1]);
        if(oidx < 0) {
            *errstr = (char *)argv[aidx];
            goto ERR;
        }
    }
    else {
        goto ERR;
    }

    if(ARGS_GET_CMD_OPT_VAL_TYPE(ops[oidx].val_type) !=
       ARGS_VAL_TYPE_NONE) {
        if(aidx + 1 >= argc) {
            *errstr = (char *)argv[aidx];
            goto ERR;
        }
        if(args_read_value(ops + oidx, argv[aidx + 1])) {
            *errstr = (char *)argv[aidx];
            goto ERR;
        }
        *idx = *idx + 1;
    }
    else {
        *((int *)ops[oidx].val) = 1;
    }
    ops[oidx].flag = 1;
    *idx = *idx + 1;

    return ops[oidx].key;

NO_MORE:
    return 0;

ERR:
    return -1;
}

static int args_set_variable_by_key_long(args_opt_t *opts, char *key_long, void *var, size_t size)
{
    int   idx;
    char  buf[ARGS_MAX_KEY_LONG];
    char *ko = key_long;
    char *kt = buf;

    /* if long key has "_", convert to "-". */
    while(*ko != '\0') {
        if(*ko == '_')
            *kt = '-';
        else
            *kt = *ko;

        ko++;
        kt++;
    }
    *kt = '\0';

    idx = args_search_long_key(opts, buf);
    if(idx < 0)
        return -1;
    opts[idx].val = var;
    opts[idx].val_size = (int)size;
    return 0;
}

static int args_set_variable_by_key(args_opt_t *opts, char *key, void *var)
{
    int idx;
    idx = args_search_short_arg(opts, key[0]);
    if(idx < 0)
        return -1;
    opts[idx].val = var;
    return 0;
}

#define ARGS_SET_PARAM_VAR_KEY_LONG(opts, param, key_long) \
    args_set_variable_by_key_long(opts, #key_long, (void *)&((param)->key_long))

#define ARGS_SET_PARAM_VAR_KEY(opts, param, key) \
    args_set_variable_by_key(opts, #key, (void *)&((param)->key))

static int args_get(args_parser_t *args, char *keyl, void **val, int *flag)
{
    int idx;

    idx = args_search_long_key(args->opts, keyl);
    if(idx >= 0) {
        if(val)
            *val = args->opts[idx].val;
        if(flag)
            *flag = args->opts[idx].flag;
        return 0;
    }
    else {
        if(val)
            *val = NULL; /* no value */
        if(flag)
            *flag = 0; /* no set */
        return -1;
    }
}

static int args_set_str(args_parser_t *args, char *keyl, char *str)
{
    int idx;

    idx = args_search_long_key(args->opts, keyl);
    if(idx >= 0) {
        if(args->opts[idx].val_size > 0) {
            snprintf((char *)(args->opts[idx].val), args->opts[idx].val_size, "%s", str);
        }
        else {
            return -1; /* STRING option without a known buffer size: reject */
        }
        args->opts[idx].flag = 1;
        return 0;
    }
    else {
        return -1;
    }
}

static int args_set_int(args_parser_t *args, char *keyl, int val)
{
    int idx;

    idx = args_search_long_key(args->opts, keyl);
    if(idx >= 0) {
        *((int *)(args->opts[idx].val)) = val;
        args->opts[idx].flag = 1;
        return 0;
    }
    else {
        return -1;
    }
}

static int args_set_int2str(args_parser_t* args, char* keyl, int val)
{
    int idx;

    idx = args_search_long_key(args->opts, keyl);
    if (idx >= 0) {
        sprintf((char*)(args->opts[idx].val), "%d", val);
        args->opts[idx].flag = 1;
        return 0;
    }
    else {
        return -1;
    }
}

static int args_set_flag(args_parser_t *args, char *keyl, int flag)
{
    int idx;

    idx = args_search_long_key(args->opts, keyl);
    if(idx >= 0) {
        args->opts[idx].flag = flag;
        return 0;
    }
    return -1;
}


static int args_get_int(args_parser_t *args, char *keyl, int *val, int *flag)
{
    int *p = NULL;
    if(args_get(args, keyl, (void **)&p, flag))
        return -1;
    if(p) {
        *val = *p;
    }
    return 0;
}

static int args_parse(args_parser_t *args, int argc, const char *argv[],
                      char **errstr)
{
    int         i, ret = 0, idx = 0;
    const char *fname_cfg = NULL;
    FILE       *fp;

    int         num_configs = 0;
    int         pos_conf_files[ARGS_MAX_NUM_CONF_FILES];
    memset(&pos_conf_files, -1, sizeof(int) * ARGS_MAX_NUM_CONF_FILES);

    /* config file parsing */
    for(i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "--" ARGS_KEY_LONG_CONFIG)) {
            if(i + 1 < argc) {
                if(num_configs >= ARGS_MAX_NUM_CONF_FILES) break;
                num_configs++;
                pos_conf_files[num_configs - 1] = i + 1;
            }
        }
    }
    for(int i = 0; i < num_configs; i++) {
        fname_cfg = argv[pos_conf_files[i]];
        if(fname_cfg) {
            fp = fopen(fname_cfg, "r");
            if(fp == NULL)
                return -1; /* config file error */

            if(args_parse_cfg(fp, args->opts, 1)) {
                fclose(fp);
                return -1; /* config file error */
            }
            fclose(fp);
        }
    }
    /* command line parsing */
    while(1) {
        ret = args_parse_cmd(argc, argv, args->opts, &idx, errstr);
        if(ret <= 0)
            break;
    }
    return ret;
}

static int args_get_help(args_parser_t *args, int idx, char *help)
{
    int         optional;
    char        vtype[32];
    args_opt_t *o = args->opts + idx;
    char        default_value[256] = { 0 };

    switch(ARGS_GET_CMD_OPT_VAL_TYPE(o->val_type)) {
    case ARGS_VAL_TYPE_INTEGER:
        strncpy(vtype, "INTEGER", sizeof(vtype) - 1);
        if(o->val != NULL)
            sprintf(default_value, " [%d]", *(int *)(o->val));
        break;
    case ARGS_VAL_TYPE_STRING:
        strncpy(vtype, "STRING", sizeof(vtype) - 1);
        if(o->val != NULL)
            snprintf(default_value, sizeof(default_value), " [%s]", strlen((char *)(o->val)) == 0 ? "None" : (char *)(o->val));
        break;
    case ARGS_VAL_TYPE_NONE:
    default:
        strncpy(vtype, "FLAG", sizeof(vtype) - 1);
        if(o->val != NULL)
            sprintf(default_value, " [%s]", *(int *)(o->val) ? "On" : "Off");
        break;
    }
    optional = !(o->val_type & ARGS_VAL_TYPE_MANDATORY);

    if(o->key != ARGS_NO_KEY) {
        snprintf(help, ARGS_HELP_BUF_SIZE, "  -%c, --%s [%s]%s%s\n    : %s", o->key, o->key_long,
                vtype, (optional) ? " (optional)" : "", (optional) ? default_value : "", o->desc);
    }
    else {
        snprintf(help, ARGS_HELP_BUF_SIZE, "  --%s [%s]%s%s\n    : %s", o->key_long,
                vtype, (optional) ? " (optional)" : "", (optional) ? default_value : "", o->desc);
    }

    return 0;
}

static int args_check_mandatory(args_parser_t *args, char **err_arg)
{
    args_opt_t *o = args->opts;

    while(o->key != 0) {
        if(o->val_type & ARGS_VAL_TYPE_MANDATORY) {
            if(o->flag == 0) {
                /* not filled all mandatory argument */
                *err_arg = o->key_long;
                return -1;
            }
        }
        o++;
    }
    return 0;
}

static void args_release(args_parser_t *args)
{
    if(args != NULL) {
        if(args->opts != NULL)
            free(args->opts);
        free(args);
    }
}

static args_parser_t *args_create(const args_opt_t *opt_table, int num_opt)
{
    args_parser_t *args = NULL;
    args_opt_t    *opts = NULL;

    args = (args_parser_t *)malloc(sizeof(args_parser_t));
    if(args == NULL)
        goto ERR;
    memset(args, 0, sizeof(args_parser_t));

    opts = (args_opt_t *)malloc(num_opt * sizeof(args_opt_t));
    if(opts == NULL)
        goto ERR;
    memcpy(opts, opt_table, num_opt * sizeof(args_opt_t));
    args->opts = opts;

    args->release = args_release;
    args->parse = args_parse;
    args->get_help = args_get_help;
    args->get_int = args_get_int;
    args->set_str = args_set_str;
    args->set_int = args_set_int;
    args->set_int2str = args_set_int2str;
    args->set_flag = args_set_flag;
    args->check_mandatory = args_check_mandatory;

    /* find actual number of options */
    args->num_option = 0;
    while(opt_table[args->num_option].key != ARGS_END_KEY)
        args->num_option++;

    return args;

ERR:
    free(opts);
    free(args);
    return NULL;
}

#endif /*_OAPV_APP_ARGS_H_ */
