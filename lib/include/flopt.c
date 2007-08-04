/** flopt.c
  *
  *     A fairly lame command-line option parser.
  *
  * Last modified: 2007-08-04
  *
  * Copyright (C) 2006-2007 James D. Lin
  *
  * This software is provided 'as-is', without any express or implied
  * warranty.  In no event will the authors be held liable for any damages
  * arising from the use of this software.
  *
  * Permission is granted to anyone to use this software for any purpose,
  * including commercial applications, and to alter it and redistribute it
  * freely, subject to the following restrictions:
  *
  * 1. The origin of this software must not be misrepresented; you must not
  *    claim that you wrote the original software. If you use this software
  *    in a product, an acknowledgment in the product documentation would be
  *    appreciated but is not required.
  * 2. Altered source versions must be plainly marked as such, and must not be
  *    misrepresented as being the original software.
  * 3. This notice may not be removed or altered from any source distribution.
  */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include "flopt.h"
#include "flopt_string.h"

typedef enum { false, true } bool;

struct flopt_context_t
{
    const flopt_option_t* optionsP;
    bool caseSensitive;

    struct {
        flopt_error_t err;
        TCHAR* optionNameP;
        TCHAR* optionValueP;
        TCHAR* messageP;
    } errorDetails;
};

typedef struct
{
    const flopt_option_t* optionP;
    const TCHAR* valP;
    TCHAR** argNextPP;
} parseState_t;


/** flopt_parse_bool
  *
  *     Parses a boolean value from the given string.
  *
  * PARAMETERS:
  *     IN valP          : A string representing a boolean value (0 or 1).
  *                        May be NULL.
  *     OUT handlerDataP : A pointer to a flopt_bool_t.
  *                        On success, set to the interpreted boolean
  *                          value.
  *                        On error, left untouched.
  *
  * RETURNS:
  *     flopt_error_none
  *     flopt_error_mismatch
  */
flopt_error_t
flopt_parse_bool(const TCHAR* valP, void* handlerDataP)
{
    flopt_error_t err = flopt_error_none;
    bool val = false;

    assert(handlerDataP != NULL);

    if (valP == NULL)
    {
        val = true;
    }
    else if (tcscmp(valP, T("0")) == 0)
    {
        val = false;
    }
    else if (tcscmp(valP, T("1")) == 0)
    {
        val = true;
    }
    else
    {
        err = flopt_error_mismatch;
    }

    if (err == flopt_error_none) { *((flopt_bool_t*) handlerDataP) = val; }
    return err;
}


/** flopt_parse_int
  *
  *     Parses an integer from the given string.
  *
  * PARAMETERS:
  *     IN valP          : A string representing a base-10 integer.
  *                        May be NULL.
  *     OUT handlerDataP : A pointer to an int.
  *                        On success, set to the interpreted integer.
  *                        On error, left untouched.
  *
  * RETURNS:
  *     flopt_error_none
  *     flopt_error_insufficient_args
  *     flopt_error_mismatch
  *     flopt_error_overflow
  *     flopt_error_unknown
  */
flopt_error_t
flopt_parse_int(const TCHAR* valP, void* handlerDataP)
{
    flopt_error_t err = flopt_error_none;
    int val = 0;
    bool matched = false;

    assert(handlerDataP != NULL);

    if (valP == NULL)
    {
        err = flopt_error_insufficient_args;
    }
    else if (valP[0] != '\0')
    {
        TCHAR* endP;
        long n;
        errno = 0;
        n = tcstol(valP, &endP, 10);

        /* Check that we matched at least one digit.
         * (strtol will return 0 if fed a string with no digits.)
         */
        if (*endP == '\0' && endP > valP)
        {
            matched = true;

            if (errno == ERANGE || n < INT_MIN || n > INT_MAX)
            {
                err = flopt_error_overflow;
                val = (n < 0) ? INT_MIN : INT_MAX;
            }
            else if (errno == 0)
            {
                val = (int) n;
            }
            else
            {
                err = flopt_error_unknown;
            }
        }
    }

    if (!matched) { err = flopt_error_mismatch; }
    if (err == flopt_error_none) { *((int*) handlerDataP) = val; }
    return err;
}


/** flopt_parse_uint
  *
  *     Parses an unsigned integer from the given string.
  *
  * PARAMETERS:
  *     IN valP          : A string representing an unsigned base-10
  *                          integer.
  *                        May be NULL.
  *     OUT handlerDataP : A pointer to an unsigned int.
  *                        On success, set to the interpreted integer.
  *                        On error, left untouched.
  *
  * RETURNS:
  *     flopt_error_none
  *     flopt_error_insufficient_args
  *     flopt_error_mismatch
  *     flopt_error_overflow
  *     flopt_error_unknown
  */
flopt_error_t
flopt_parse_uint(const TCHAR* valP, void* handlerDataP)
{
    flopt_error_t err = flopt_error_none;
    int val = 0;
    bool matched = false;

    assert(handlerDataP != NULL);

    if (valP == NULL)
    {
        err = flopt_error_insufficient_args;
    }
    else if (valP[0] != '\0' && valP[0] != T('-'))
    {
        TCHAR* endP;
        unsigned long n;
        errno = 0;
        n = tcstoul(valP, &endP, 10);

        /* Check that we matched at least one digit.
         * (strtol will return 0 if fed a string with no digits.)
         */
        if (*endP == '\0' && endP > valP)
        {
            matched = true;

            if (errno == ERANGE || n > UINT_MAX)
            {
                err = flopt_error_overflow;
                val = UINT_MAX;
            }
            else if (errno == 0)
            {
                val = (unsigned int) n;
            }
            else
            {
                err = flopt_error_unknown;
            }
        }
    }

    if (!matched) { err = flopt_error_mismatch; }
    if (err == flopt_error_none) { *((unsigned int*) handlerDataP) = val; }
    return err;
}


/** flopt_parse_double
  *
  *     Parses a double from the given string.
  *
  * PARAMETERS:
  *     IN valP          : A string representing a base-10 floating-point
  *                          number.
  *                        May be NULL.
  *     OUT handlerDataP : A pointer to a double.
  *                        On success, set to the interpreted double.
  *                        On error, left untouched.
  *
  * RETURNS:
  *     flopt_error_none
  *     flopt_error_insufficient_args
  *     flopt_error_mismatch
  *     flopt_error_overflow
  *     flopt_error_unknown
  */
flopt_error_t
flopt_parse_double(const TCHAR* valP, void* handlerDataP)
{
    flopt_error_t err = flopt_error_none;
    double val = 0.0;
    bool matched = false;

    assert(handlerDataP != NULL);

    if (valP == NULL)
    {
        err = flopt_error_insufficient_args;
    }
    else if (valP[0] != '\0')
    {
        TCHAR* endP;
        errno = 0;
        val = tcstod(valP, &endP);

        /* Check that we matched at least one digit.
         * (strtod will return 0 if fed a string with no digits.)
         */
        if (*endP == '\0' && endP > valP)
        {
            matched = true;

            if (errno == ERANGE)
            {
                err = flopt_error_overflow;
            }
            else if (errno != 0)
            {
                err = flopt_error_unknown;
            }
        }
    }

    if (!matched) { err = flopt_error_mismatch; }
    if (err == flopt_error_none) { *((double*) handlerDataP) = val; }
    return err;
}


/** flopt_parse_string
  *
  *     Obtains a string.
  *
  * PARAMETERS:
  *     IN valP          : A string.
  *                        May be NULL.
  *     OUT handlerDataP : A pointer to pointer-to-char.
  *                        On success, set to the input string.
  *                        On error, left untouched.
  *
  * RETURNS:
  *     flopt_error_none
  *     flopt_error_insufficient_args
  */
flopt_error_t
flopt_parse_string(const TCHAR* valP, void* handlerDataP)
{
    flopt_error_t err = flopt_error_none;

    assert(handlerDataP != NULL);

    if (valP == NULL)
    {
        err = flopt_error_insufficient_args;
    }

    if (err == flopt_error_none) { *((const TCHAR**) handlerDataP) = valP; }
    return err;
}


/** isValidOption
  *
  * PARAMETERS:
  *     IN optionP : Specification for an individual option.
  *
  * RETURNS:
  *     true if the specified option is valid, false if it's a sentinel
  *       value.
  */
static bool
isValidOption(const flopt_option_t* optionP)
{
    return    optionP != NULL
           && !(   optionP->longName == NULL
                && optionP->shortName == '\0');
}


/** findOptionLong
  *
  *     Finds a the option specification for a "long" option (i.e., an
  *     option of the form "--option").
  *
  * PARAMETERS:
  *     IN optionsP   : The list of option specifications.
  *     IN longNameP  : The "long" option to search for.
  *     caseSensitive : Pass true to use case-sensitive comparisons, false
  *                       otherwise.
  *
  * RETURNS:
  *     A pointer to the corresponding option specification or NULL if not
  *       found.
  */
static const flopt_option_t*
findOptionLong(const flopt_option_t* optionsP, const TCHAR* longNameP, bool caseSensitive)
{
    const flopt_option_t* optionP;
    int (*cmp)(const TCHAR*, const TCHAR*) = caseSensitive ? tcscmp : flopt_stricmp;

    assert(optionsP != NULL);
    assert(longNameP != NULL);
    for (optionP = optionsP; isValidOption(optionP); optionP++)
    {
        if (   optionP->longName != NULL
            && cmp(longNameP, optionP->longName) == 0)
        {
            return optionP;
        }
    }
    return NULL;
}


/** findOptionShort
  *
  *     Finds a the option specification for a "short" option (i.e., an
  *     option of the form "-o").
  *
  * PARAMETERS:
  *     IN optionsP   : The list of option specifications.
  *     shortName     : The "short" option to search for.
  *     caseSensitive : Pass true to use case-sensitive comparisons, false
  *                       otherwise.
  *
  * RETURNS:
  *     A pointer to the corresponding option specification or NULL if not
  *       found.
  */
static const flopt_option_t*
findOptionShort(const flopt_option_t* optionsP, TCHAR shortName, bool caseSensitive)
{
    const flopt_option_t* optionP;
    assert(optionsP != NULL);
    assert(shortName != '\0');
    for (optionP = optionsP; isValidOption(optionP); optionP++)
    {
        if (   shortName == optionP->shortName
            || (!caseSensitive && tclower(shortName) == tclower(optionP->shortName)))
        {
            return optionP;
        }
    }
    return NULL;
}


/** flopt_set_error_details
  *
  *     Generates error details in the options context.
  *
  * PARAMETERS:
  *     IN/OUT contextP : The options context.
  *     err             : The error code.
  *     IN optionNameP  : The name of the option we failed on.
  *     IN optionValueP : The value of the option we failed on.
  *                       Pass NULL if unwanted.
  */
static void
flopt_set_error_details(flopt_context_t* contextP, flopt_error_t err,
                        const TCHAR* optionNameP, const TCHAR* optionValueP)
{
    TCHAR* s = NULL;

    assert(contextP != NULL);
    assert(optionNameP != NULL);

    switch (err)
    {
        /* These aren't really errors. */
        case flopt_error_none:
            break;
        case flopt_error_cancel:
            err = flopt_error_none;
            break;

        case flopt_error_invalid:
            s = flopt_aprintf(T("Invalid option: %s"), optionNameP);
            break;
        case flopt_error_insufficient_args:
            s = flopt_aprintf(T("Value required after option %s"), optionNameP);
            break;
        case flopt_error_mismatch:
            if (optionValueP == NULL)
            {
                s = flopt_aprintf(T("Invalid value for option %s"), optionNameP);
            }
            else
            {
                s = flopt_aprintf(T("Invalid value for option %s: %s"),
                                  optionNameP, optionValueP);
            }
            break;
        case flopt_error_overflow:
            if (optionValueP == NULL)
            {
                s = flopt_aprintf(T("Integer overflow for option %s"), optionNameP);
            }
            else
            {
                s = flopt_aprintf(T("Integer overflow for option %s: %s"),
                                  optionNameP, optionValueP);
            }
            break;
        case flopt_error_custom:
            break;
        case flopt_error_unknown:
        default:
            s = flopt_aprintf(T("Unknown error handling option %s."), optionNameP);
            break;
    }

    if (err != flopt_error_custom) /* Leave custom error messages alone. */
    {
        flopt_set_error_message(contextP, s);
        free(s);
    }

    contextP->errorDetails.err = err;

    free(contextP->errorDetails.optionNameP);
    free(contextP->errorDetails.optionValueP);

    contextP->errorDetails.optionNameP = flopt_strdup(optionNameP);
    contextP->errorDetails.optionValueP = optionValueP != NULL
                                          ? flopt_strdup(optionValueP)
                                          : NULL;
}


/** setShortOptionErrorDetails
  *
  *     Generates error details in the options context.
  *
  * PARAMETERS:
  *     IN/OUT contextP : The options context.
  *     err             : The error code.
  *     shortName       : the "short" name of the option we failed on.
  *     IN optionValueP : The value of the option we failed on.
  *                       Pass NULL if unwanted.
  */
static void
setShortOptionErrorDetails(flopt_context_t* contextP, flopt_error_t err,
                           TCHAR shortName, const TCHAR* optionValueP)
{
    TCHAR shortNameBuf[3] = T("-?");

    assert(shortName != '\0');

    shortNameBuf[1] = shortName;

    flopt_set_error_details(contextP, err, shortNameBuf, optionValueP);
}


/** flopt_get_error
  *
  * PARAMETERS:
  *     IN contextP : The options context.
  *
  * RETURNS:
  *     The current error code waiting in the options context.
  */
flopt_error_t
flopt_get_error(const flopt_context_t* contextP)
{
    assert(contextP != NULL);
    return contextP->errorDetails.err;
}


/** flopt_get_error_details
  *
  *     Retrieves details about the current error.
  *
  * PARAMETERS:
  *     IN contextP       : The options context.
  *     OUT optionNamePP  : On output, the name of the option we failed on.
  *                           May be set to NULL.
  *                         Pass NULL if unwanted.
  *     OUT optionValuePP : On output, the value of the option we failed on.
  *                           May be set to NULL.
  */
void
flopt_get_error_details(const flopt_context_t* contextP,
                        TCHAR** optionNamePP, TCHAR** optionValuePP)
{
    if (optionNamePP != NULL)
    {
        *optionNamePP = contextP->errorDetails.optionNameP;
    }

    if (optionValuePP != NULL)
    {
        *optionValuePP = contextP->errorDetails.optionValueP;
    }
}


/** flopt_set_error_message
  *
  *     Sets a custom error message in the options context.
  *
  * PARAMETERS:
  *     IN/OUT contextP : The options context.
  *     IN messageP     : The error message.
  */
void
flopt_set_error_message(flopt_context_t* contextP, const TCHAR* messageP)
{
    TCHAR* oldMessageP = contextP->errorDetails.messageP;
    TCHAR* s = NULL;

    assert(contextP != NULL);

    if (messageP != NULL) { s = flopt_strdup(messageP); }

    contextP->errorDetails.err = flopt_error_custom;
    contextP->errorDetails.messageP = s;
    free(oldMessageP);
}


/** flopt_get_error_message
  *
  * PARAMETERS:
  *     IN contextP : The options context.
  *
  * RETURNS:
  *     The current error message waiting in the options context or the
  *       empty string if there are no errors.
  */
const TCHAR*
flopt_get_error_message(const flopt_context_t* contextP)
{
    assert(contextP != NULL);
    return (contextP->errorDetails.messageP == NULL)
           ? T("")
           : contextP->errorDetails.messageP;
}


/** flopt_get_help
  *
  * PARAMETERS:
  *     IN optionsP : The list of option specifications.
  *     compact     : Pass false to include blank lines between options.
  *
  * RETURNS:
  *     An allocated help string for the available options.  The caller is
  *       responsible for calling free() on it when no longer needed.
  */
TCHAR*
flopt_get_help(const flopt_option_t* optionsP, flopt_bool_t compact)
{
    TCHAR* helpTextP = NULL;
    flopt_stringstream* ssP = flopt_ssopen();

    assert(optionsP != NULL);

    if (ssP != NULL)
    {
        static const int maxWidth = 4;
        const flopt_option_t* optionP;

        for (optionP = optionsP; isValidOption(optionP); optionP++)
        {
            int n = 0;

            /* Undocumented option.  Ignore it and move on. */
            if (optionP->description == NULL || optionP->attr & flopt_attr_hidden)
            {
                continue;
            }

            if (optionP->longName != NULL && optionP->shortName != '\0')
            {
                /* Both shortName and longName */
                n = flopt_ssprintf(ssP, T("  -%c, --%s"), optionP->shortName, optionP->longName);
            }
            else if (optionP->longName != NULL)
            {
                /* longName only */
                n = flopt_ssprintf(ssP, T("  --%s"), optionP->longName);
            }
            else if (optionP->shortName != '\0')
            {
                /* shortName only */
                n = flopt_ssprintf(ssP, T("  -%c"), optionP->shortName);
            }
            else
            {
                assert(!"No option name specified.");
                break;
            }

            if (n < 0) { n = 0; }

            if (optionP->argDescription != NULL)
            {
                int m = flopt_ssprintf(ssP,
                                       (optionP->attr & flopt_attr_optional)
                                       ? T("[=%s]")
                                       : T("=%s"),
                                       optionP->argDescription);
                if (m > 0) { n += m; }
            }

            if (n > maxWidth)
            {
                flopt_ssprintf(ssP, T("\n"));
                n = 0;
            }
            flopt_ssprintf(ssP, T("%*s  %s\n"),
                           maxWidth - n, T(""),
                           optionP->description);
            if (!compact) { flopt_ssprintf(ssP, T("\n")); }
        }
        helpTextP = flopt_ssfinalize(ssP);
        flopt_ssclose(ssP);
    }

    return helpTextP;
}


/** flopt_print_help
  *
  *     Prints help for the available options.
  *
  * PARAMETERS:
  *     IN/OUT fP   : The file stream to print to.
  *     IN optionsP : The list of option specifications.
  *     compact     : Pass false to include blank lines between options.
  */
void
flopt_print_help(FILE* fp, const flopt_option_t* optionsP, flopt_bool_t compact)
{
    TCHAR* helpTextP = flopt_get_help(optionsP, compact);
    if (helpTextP != NULL)
    {
        fputs(helpTextP, fp);
        free(helpTextP);
    }
}


/** set
  *
  *     Sets the value for a specified option by invoking the option's
  *     handler callback.
  *
  * PARAMETERS:
  *     IN optionP : The option.
  *     IN valP    : The option's value.  May be NULL.
  *
  * RETURNS:
  *     An error code.
  */
static flopt_error_t
set(const flopt_option_t* optionP, const TCHAR* valP)
{
    flopt_error_t err = flopt_error_none;

    assert(optionP != NULL);

    if (optionP->handler != NULL)
    {
        err = optionP->handler(valP, optionP->handlerDataP);
    }
    else
    {
        assert(!"No option handler specified.");
    }
    return err;
}


/** parseArg
  *
  *     Helper function to flopt_parse to deal with consuming possibly
  *     optional arguments.
  *
  * PARAMETERS:
  *     IN/OUT psP : The current parse state.
  *
  * RETURNS:
  *     An error code.
  */
static flopt_error_t
parseArg(parseState_t* psP)
{
    flopt_error_t err = flopt_error_none;

    bool consumeNextArg = false;

    if (   psP->optionP->argDescription != NULL
        && psP->valP == NULL
        && *(psP->argNextPP) != NULL)
    {
        consumeNextArg = true;
        psP->valP = *(psP->argNextPP);
    }

    /* Even for options that don't ask for arguments, always parse and
     * consume an argument that was specified with '='.
     */
    err = set(psP->optionP, psP->valP);

    if (   err != flopt_error_none
        && (psP->optionP->attr & flopt_attr_optional)
        && consumeNextArg
        && psP->valP != NULL)
    {
        /* Try again. */
        consumeNextArg = false;
        psP->valP = NULL;
        err = set(psP->optionP, NULL);
    }

    if (err == flopt_error_none && consumeNextArg)
    {
        psP->argNextPP++;
    }
    return err;
}


/** flopt_parse
  *
  *     Parses command-line options.
  *
  * PARAMETERS:
  *     IN contextP : The options context.
  *     IN/OUT argv : The list of command-line arguments, not including the
  *                     initial program name.  Must be terminated with a
  *                     NULL sentinel value.
  *                   Note that the command-line arguments might be
  *                     mutated in the process.
  *
  * RETURNS:
  *     A pointer to the first unprocessed element in argv.
  *     Never returns NULL.
  */
TCHAR**
flopt_parse(flopt_context_t* contextP,
             TCHAR** argv)
{
    flopt_error_t err = flopt_error_none;

    TCHAR* argP;
    parseState_t ps;

    assert(contextP != NULL);
    assert(argv != NULL);

    ps.optionP = NULL;
    ps.valP = NULL;
    ps.argNextPP = argv;

    while (   (argP = *ps.argNextPP) != NULL
           && argP[0] == T('-'))
    {
        assert(err == flopt_error_none);

        if (argP[1] == '\0')
        {
            /* - */
            goto abort;
        }

        ps.argNextPP++;

        if (argP[1] == T('-'))
        {
            TCHAR* argNameP = argP + 2;
            if (argNameP[0] == '\0')
            {
                /* -- */
                goto abort;
            }
            else
            {
                /* --longName */
                {
                    TCHAR* p = tcschr(argNameP, T('='));
                    if (p != NULL)
                    {
                        *p = '\0';
                        ps.valP = p + 1;
                    }
                }

                ps.optionP = findOptionLong(contextP->optionsP, argNameP,
                                            contextP->caseSensitive);
                if (ps.optionP == NULL)
                {
                    err = flopt_error_invalid;
                    flopt_set_error_details(contextP, err, argP, NULL);
                    goto abort;
                }
                else
                {
                    err = parseArg(&ps);
                    if (err != flopt_error_none)
                    {
                        flopt_set_error_details(contextP, err, argP, ps.valP);
                        goto abort;
                    }
                }

                if (ps.optionP->attr & flopt_attr_halt) { goto abort; }
            }
        }
        else
        {
            size_t len;
            size_t j;

            {
                const TCHAR* p = tcschr(argP, T('='));
                if (p == NULL)
                {
                    len = tcslen(argP);
                    ps.valP = NULL;
                }
                else
                {
                    len = p - argP;
                    ps.valP = p + 1;
                }
            }

            for (j = 1; j < len; j++)
            {
                ps.optionP = findOptionShort(contextP->optionsP, argP[j],
                                            contextP->caseSensitive);
                if (ps.optionP == NULL)
                {
                    err = flopt_error_invalid;
                    setShortOptionErrorDetails(contextP, err, argP[j], NULL);
                    goto abort;
                }
                else
                {
                    if (j + 1 == len)
                    {
                        /* The last short option in a condensed list gets
                         * use an argument.
                         */
                        err = parseArg(&ps);
                        if (err != flopt_error_none)
                        {
                            setShortOptionErrorDetails(contextP, err, argP[j], ps.valP);
                            goto abort;
                        }
                    }
                    else if (   ps.optionP->argDescription == NULL
                             || (ps.optionP->attr & flopt_attr_optional))
                    {
                        err = set(ps.optionP, NULL);
                        if (err != flopt_error_none)
                        {
                            setShortOptionErrorDetails(contextP, err, argP[j], NULL);
                            goto abort;
                        }
                    }
                    else
                    {
                        /* Short options with required arguments can't be
                         * used in condensed lists except in the last
                         * position.
                         *
                         * e.g. -abcd arg
                         *          ^
                         */
                        err = flopt_error_insufficient_args;
                        setShortOptionErrorDetails(contextP, err, argP[j], NULL);
                    }
                }

                if (ps.optionP->attr & flopt_attr_halt) { goto abort; }
            }
        }

        ps.optionP = NULL;
        ps.valP = NULL;
    }

abort:
    return ps.argNextPP;
}


/** flopt_new_context
  *
  *     Creates a new options context.
  *
  * RETURNS:
  *     An allocated options context.  The caller is responsible for
  *       freeing it with flopt_free_context when no longer needed.
  *     Returns NULL on error.
  */
flopt_context_t*
flopt_new_context(void)
{
    flopt_context_t* contextP = malloc(sizeof *contextP);
    if (contextP != NULL)
    {
        contextP->optionsP = NULL;
        contextP->caseSensitive = true;
        contextP->errorDetails.err = flopt_error_none;
        contextP->errorDetails.optionNameP = NULL;
        contextP->errorDetails.optionValueP = NULL;
        contextP->errorDetails.messageP = NULL;
    }
    return contextP;
}


/** flopt_free_context
  *
  *     Frees an options context.
  *
  * PARAMETERS:
  *     IN/OUT contextP : The options context to free.
  */
void
flopt_free_context(flopt_context_t* contextP)
{
    if (contextP != NULL)
    {
        free(contextP->errorDetails.optionNameP);
        free(contextP->errorDetails.optionValueP);
        free(contextP->errorDetails.messageP);

        free(contextP);
    }
}


/** flopt_set_options
  *
  *     Specifies a list of options to use with an options context.
  *
  * PARAMETERS:
  *     IN/OUT contextP : The options context.
  *     IN optionsP     : The list of option specifications.
  */
void
flopt_set_options(flopt_context_t* contextP, const flopt_option_t* optionsP)
{
    assert(contextP != NULL);
    contextP->optionsP = optionsP;
}


/** flopt_set_case_sensitive
  *
  *     Specifies whether options should be case-sensitive. (Options are
  *     case-sensitive by default.)
  *
  * PARAMETERS:
  *     IN/OUT contextP : The options context.
  *     caseSensitive   : Pass 1 if options should be case-sensitive,
  *                         0 otherwise.
  */
void
flopt_set_case_sensitive(flopt_context_t* contextP, flopt_bool_t caseSensitive)
{
    assert(contextP != NULL);
    contextP->caseSensitive = (caseSensitive != 0);
}
