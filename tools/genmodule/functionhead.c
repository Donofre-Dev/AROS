/*
    Copyright (C) 1995-2015, The AROS Development Team. All rights reserved.

    The code for storing information of functions present in the module
*/
#include <string.h>
#include <assert.h>
#include "functionhead.h"
#include "config.h"

/* [] at the end will be added as * in the variable type
 * e.g. char *var[] => type: char **, name: var
 */
void parsetypeandname(struct functionarg *funcarg);

struct functionhead *newfunctionhead(const char *name, enum libcall libcall)
{
    struct functionhead *funchead = malloc(sizeof(struct functionhead));

    if (funchead != NULL)
    {
        funchead->next = NULL;
        funchead->internalname = funchead->name = strdup(name);
        funchead->type = NULL;
        funchead->libcall = libcall;
        funchead->lvo = 0;
        funchead->argcount = 0;
        funchead->arguments = NULL;
        funchead->aliases = NULL;
        funchead->interface = NULL;
        funchead->method = NULL;
        funchead->varargtype = 0;
        funchead->novararg = 0;
        funchead->priv= 0;
        funchead->unusedlibbase = 0;
    }
    else
    {
        puts("Out of memory !");
        exit(20);
    }

    return funchead;
}

struct functionarg *funcaddarg
(
    struct functionhead *funchead,
    const char *arg, const char *reg
)
{
    struct functionarg **argptr = &funchead->arguments;

    while ((*argptr) != NULL) argptr = &(*argptr)->next;

    *argptr = malloc(sizeof(struct functionarg));
    if (*argptr != NULL)
    {
        (*argptr)->next = NULL;
        (*argptr)->arg  = (arg == NULL) ? NULL : strdup(arg);
        (*argptr)->type = NULL;
        (*argptr)->name = NULL;
        (*argptr)->reg  = (reg  == NULL) ? NULL : strdup(reg);
        (*argptr)->parent  = funchead;
        (*argptr)->ellipsis = 0;

        parsetypeandname((*argptr));

        funchead->argcount++;
    }
    else
    {
        puts("Out of memory !");
        exit(20);
    }

    return *argptr;
}

struct stringlist *funcaddalias(struct functionhead *funchead, const char *alias)
{
    return slist_append(&funchead->aliases, alias);
}

void funcsetinternalname(struct functionhead *funchead, const char *internalname)
{
    if (funchead->name != funchead->internalname)
        free(funchead->internalname);
    funchead->internalname = strdup(internalname);
}

void writefuncdefs(FILE *out, struct config *cfg, struct functionhead *funclist)
{
    struct functionhead *funclistit;
    struct functionarg *arglistit;
    int first;

    for(funclistit = funclist; funclistit != NULL; funclistit = funclistit->next)
    {
        switch (funclistit->libcall)
        {
        case STACK:
            fprintf(out, "%s %s(", funclistit->type, funclistit->internalname);

            for(arglistit = funclistit->arguments, first = 1;
                arglistit != NULL;
                arglistit = arglistit->next, first = 0
            )
            {
                if (!first)
                    fprintf(out, ", ");

                fprintf(out, "%s", arglistit->arg);
            }
            fprintf(out, ");\n");
            break;

        case REGISTER:
            assert(cfg);
            fprintf(out, "%s %s(", funclistit->type, funclistit->internalname);
            for (arglistit = funclistit->arguments, first = 1;
                 arglistit!=NULL;
                 arglistit = arglistit->next, first = 0
            )
            {
                if (!first)
                    fprintf(out, ", ");
                fprintf(out, "%s", arglistit->arg);
            }
            fprintf(out,
                    ");\nAROS_LH%d%s(%s, %s,\n",
                    funclistit->argcount, funclistit->unusedlibbase ? "I" : "",
                    funclistit->type, funclistit->internalname
            );
            for (arglistit = funclistit->arguments;
                 arglistit!=NULL;
                 arglistit = arglistit->next
            )
            {
                assert(arglistit->name != NULL && arglistit->type != NULL);

                fprintf(out,
                        "         AROS_LHA(%s, %s, %s),\n",
                        arglistit->type, arglistit->name, arglistit->reg
                );
            }
            fprintf(out,
                    "         %s, %s, %u, %s)\n"
                    "{\n"
                    "    AROS_LIBFUNC_INIT\n\n"
                    "    %s%s(",
                    cfg->libbasetypeptrextern, cfg->libbase, funclistit->lvo, cfg->basename,
                    (strcasecmp(funclistit->type, "void") == 0) ? "" : "return ",
                    funclistit->internalname
            );
            for (arglistit = funclistit->arguments, first = 1;
                 arglistit!=NULL;
                 arglistit = arglistit->next, first = 0
            )
            {
                assert(arglistit->name != NULL);

                if (!first)
                    fprintf(out, ", ");
                fprintf(out, "%s", arglistit->name);
            }
            fprintf(out,
                    ");\n\n"
                    "    AROS_LIBFUNC_EXIT\n"
                    "}\n\n");
            break;

        case REGISTERMACRO:
            assert(cfg);
            if (funclistit->arguments == NULL
                || strchr(funclistit->arguments->reg, '/') == NULL)
            {
                fprintf(out,
                        "AROS_LD%d%s(%s, %s,\n",
                        funclistit->argcount, funclistit->unusedlibbase ? "I" : "",
                        funclistit->type, funclistit->internalname
                );
                for (arglistit = funclistit->arguments;
                     arglistit!=NULL;
                     arglistit = arglistit->next
                )
                {
                    assert(arglistit->type != NULL && arglistit->name != NULL);

                    fprintf(out,
                            "         AROS_LDA(%s, %s, %s),\n",
                            arglistit->type, arglistit->name, arglistit->reg
                    );
                }
                fprintf(out,
                        "         LIBBASETYPEPTR, %s, %u, %s\n"
                        ");\n",
                        cfg->libbase, funclistit->lvo, cfg->basename
                );
            }
            else
            {
                fprintf(out,
                        "AROS_LDDOUBLE%d(%s, %s,\n",
                        funclistit->argcount, funclistit->type, funclistit->internalname
                );
                for (arglistit = funclistit->arguments;
                     arglistit != NULL;
                     arglistit = arglistit->next
                )
                {
                    if (strlen(arglistit->reg) != 5)
                    {
                        fprintf(stderr, "Internal error: ../.. register format expected\n");
                        exit(20);
                    }
                    arglistit->reg[2] = 0;

                    assert(arglistit->type != NULL && arglistit->name != NULL);

                    fprintf(out,
                            "         AROS_LDA2(%s, %s, %s, %s),\n",
                            arglistit->type, arglistit->name, arglistit->reg, arglistit->reg+3
                    );
                    arglistit->reg[2] = '/';
                }
                fprintf(out,
                        "         LIBBASETYPEPTR, %s, %u, %s\n"
                        ");\n",
                        cfg->libbase, funclistit->lvo, cfg->basename
                );
            }
            break;

        default:
            fprintf(stderr, "Internal error: unhandled libcall in writefuncdefs\n");
            exit(20);
            break;
        }
    }
}

void writefuncprotos(FILE *out, struct config *cfg, struct functionhead *funclist)
{
    struct functionhead *funclistit;
    struct functionarg *arglistit;
    int first;

    for(funclistit = funclist; funclistit != NULL; funclistit = funclistit->next)
    {
        if (!funclistit->priv && (funclistit->lvo >= cfg->firstlvo))
        {
            fprintf(out,
                    "\n"
                    "#if !defined(__%s_LIBAPI__) || (%d <= __%s_LIBAPI__)"
                    "\n",
                    cfg->includenameupper,
                    funclistit->version,
                    cfg->includenameupper
            );

            switch (funclistit->libcall)
            {
            case STACK:
                fprintf(
                    out,
                    "#ifndef %s\n"
                    "%s %s(",
                    funclistit->name,
                    funclistit->type, funclistit->name
                );

                for(arglistit = funclistit->arguments, first = 1;
                    arglistit != NULL;
                    arglistit = arglistit->next, first = 0
                )
                {
                    if (!first)
                        fprintf(out, ", ");

                    fprintf(out, "%s", arglistit->arg);
                }
                fprintf(out, ");\n#endif\n");
                break;

            case REGISTER:
            case REGISTERMACRO:
                assert(cfg);
                if (funclistit->arguments == NULL
                    || strchr(funclistit->arguments->reg, '/') == NULL
                )
                {
                    fprintf(out,
                            "AROS_LP%d%s(%s, %s,\n",
                            funclistit->argcount, funclistit->unusedlibbase ? "I" : "",
                            funclistit->type, funclistit->name
                    );
                    for (arglistit = funclistit->arguments;
                        arglistit!=NULL;
                        arglistit = arglistit->next
                    )
                    {
                        assert(arglistit->type != NULL && arglistit->name != NULL);

                        fprintf(out,
                                "         AROS_LPA(%s, %s, %s),\n",
                                arglistit->type, arglistit->name, arglistit->reg
                        );
                    }
                    fprintf(out,
                            "         LIBBASETYPEPTR, %s, %u, %s\n"
                            ");\n",
                            cfg->libbase, funclistit->lvo, cfg->basename
                    );
                }
                else
                {
                    fprintf(out,
                            "AROS_LPDOUBLE%d(%s, %s,\n",
                            funclistit->argcount, funclistit->type, funclistit->name
                    );
                    for (arglistit = funclistit->arguments;
                        arglistit != NULL;
                        arglistit = arglistit->next
                    )
                    {
                        if (strlen(arglistit->reg) != 5)
                        {
                            fprintf(stderr, "Internal error: ../.. register format expected\n");
                            exit(20);
                        }
                        arglistit->reg[2] = 0;

                        assert(arglistit->type != NULL && arglistit->name != NULL);

                        fprintf(out,
                                "         AROS_LPA2(%s, %s, %s, %s),\n",
                                arglistit->type, arglistit->name, arglistit->reg, arglistit->reg+3
                        );
                        arglistit->reg[2] = '/';
                    }
                    fprintf(out,
                            "         LIBBASETYPEPTR, %s, %u, %s\n"
                            ");\n",
                            cfg->libbase, funclistit->lvo, cfg->basename
                    );
                }
                break;

            default:
                fprintf(stderr, "Internal error:"
                    " unhandled libcall in writefuncprotos\n");
                exit(20);
                break;
            }

            fprintf(out,
                    "\n"
                    "#endif /* !defined(__%s_LIBAPI__) || (%d <= __%s_LIBAPI__) */"
                    "\n",
                    cfg->includenameupper,
                    funclistit->version,
                    cfg->includenameupper
            );
        }
    }
}

void writefuncinternalstubs(FILE *out, struct config *cfg, struct functionhead *funclist)
{
    struct functionhead *funclistit;

    for(funclistit = funclist; funclistit != NULL; funclistit = funclistit->next)
    {
        switch (funclistit->libcall)
        {
        case STACK:
            if ((cfg->options & OPTION_DUPBASE)
                && !funclistit->unusedlibbase
            )
            {
                fprintf(out,
                        "AROS_GM_STACKCALL(%s,%s,%d);\n"
                        , funclistit->internalname
                        , cfg->basename
                        , funclistit->lvo
                );
            }
            else
            {
                fprintf(out,
                        "AROS_GM_STACKALIAS(%s,%s,%d);\n"
                        , funclistit->internalname
                        , cfg->basename
                        , funclistit->lvo
                );
            }
            break;

        case REGISTER:
        case REGISTERMACRO:
            /* NOP */
            break;

        default:
            fprintf(stderr, "Internal error: "
                "unhandled libcall in writefuncinternalstubs\n");
            exit(20);
            break;
        }
    }
}

void parsetypeandname(struct functionarg *funcarg)
{
    char *s, *begin, *end, *endname;
    unsigned int brackets = 0, i;
    int len = 0;

    begin = s = strdup(funcarg->arg);

    /* Count the [] at the end of the argument */
    end = begin+strlen(begin);
    while (isspace(*(end-1))) end--;

    while (*(end-1)==']')
    {
        brackets++;
        end--;
        while (isspace(*(end-1)) || isalnum(*(end-1))) end--;
        if (*(end-1)!='[')
        {
            free(s);
            fprintf(stderr, "unmatched brackets found for arg: %s in function %s\n", funcarg->arg, funcarg->parent->name);
            exit(20);
        }
        end--;
        while (isspace(*(end-1))) end--;
    }

    /* Pointer to function argument requires special handling */
    if (*(end-1)==')')
    {
        end = begin;
        while (*(end)!='(') end++;
        while (*(end)!='*') end++;
        end++;
        endname=end;
        while (*(endname)!=')') endname++;
        /* end - endname => name */
        len = endname-end;
        funcarg->name = malloc(len+1);
        strncpy(funcarg->name, end, len);
        funcarg->name[len] = '\0';

        /* Copy rest of function pointer type over name is s */
        while ((*endname)!='\0')
        {
            *end=*endname;
            end++;endname++;
        }
        *end='\0';
        funcarg->type = strdup(s);

        free(s);
        return;
    }

    /* Regular argument */
    endname = end;
    while (!isspace(*(end-1)) && *(end-1)!='*')
    {
        if (begin == end)
        {
            /* Support special cases */
            if ((strcmp(s, "void") == 0) || (strcmp(s, "...") == 0))
            {
                funcarg->type = strdup(s);
                funcarg->name = strdup("");
                if (strcmp(s, "...") == 0) funcarg->ellipsis = 1;

                free(s);
                return;
            }
            else
            {
                if (funcarg->reg == NULL)
                {
                    /* Allow empty argument names for non-reg-call functions */
                    funcarg->type = strdup(s);
                    funcarg->name = strdup("noname");
                    free(s);
                    return;
                }
                else
                {
                    free(s);
                    fprintf(stderr, "no argument type or name found for arg: %s in function %s\n", funcarg->arg, funcarg->parent->name);
                    exit(20);
                }
            }
        }
        end--;
    }

    /* begin - end => type, end - endname => name */
    len = endname-end;
    funcarg->name = malloc(len+1);
    strncpy(funcarg->name, end, len);
    funcarg->name[len] = '\0';

    /* Add * for the brackets */
    while (isspace(*(end-1))) end--;
    for (i=0; i<brackets; i++)
    {
        *end='*';
        end++;
    }
    *end='\0';
    if (funcarg->type == NULL) funcarg->type = strdup(s);

    free(s);
    return;
}
