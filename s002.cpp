
#ifndef _MUGEO_ENV_H_
#define _MUGEO_ENV_H_

#include <string>

namespace env
{
    namespace opt
    {
        void reset();
        int getopt(int argc, char *argv[], char *optstring);
    }

    struct c_confstr {
        std::string _str;
        template <typename T>
        T to() {
            T t;
            std::istringstream in(_str);
            in >> t;
            return t;
        }
        const char * str() { return _str.c_str(); }
    };

    class c_config
    {
    private:
        c_config() {}
        ~c_config() {}
        c_config(const c_config &);
        c_config& operator=(const c_config&);
    public:
        c_confstr operator()(const std::string & name, const std::string & default); 
        c_confstr operator()(const std::string & name) { return (*this)(name, ""); }

    };
    // singleton
    static c_config & get_config();

    enum { 
        Error = 0, 
        Warn = 1, 
        Info = 2,
        Debug = 3,
        Trace = 4,
        Verbose = 5
    };
    class c_log
    {
    private:
        std::string _prefix;
        void * _impl;       // reserve for future usage
    public:
        c_log(const std::string & prefix = "");
        ~c_log();

        c_log & operator()(int level, const std::string & msg);
        c_log & operator()(const std::string & msg) { return (*this)(Info, msg); }
        c_log & operator()(int level, const char * fmt, ...);
        c_log & operator()(const char * fmt, ...);

        // VC++ not support variadic parameters
        // template <typename... Args>
        // c_log & printf(int level, const char * fmt, Args... args) { return (*this)(format(level, fmt, args...); }

        // TODO, use lambda to speed output

        // singleton
        static c_log & get_log();
    };

    // format output
    std::string format(const char * fmt, ...);

    // load config file, initialize log system
    void init_env(int argc, char * argv[]);

    // for testing
    void test_opts();
    void test_log();
    void test_misc();
};

#endif  // _MUGEO_ENV_H_





#include "env.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <string.h>

/* Get va_list.  */
#if __STDC__ || defined __cplusplus || defined _MSC_VER
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#define NULL 0
#define EOF     (-1)

namespace env
{
    class exception : public std::exception {
    public:
        std::string _what;
        exception(const std::string & msg) : _what("env::exception : " + msg) {}
        const char * what() { return _what.c_str(); }
    };

    // getopt code is from XGetopt
    // http://www.codeproject.com/Articles/1940/XGetopt-A-Unix-compatible-getopt-for-MFC-and-Win
    namespace opt
    {
        static char *optarg = NULL;		// global argument pointer
        static int  optind = 0; 	// global argv index
        static char *next = NULL;

        void reset()
        {
            optarg = NULL;
            optind = 0;
            next = NULL;
        }

        int getopt(int argc, char *argv[], char *optstring)
        {
            if (optind == 0)
                next = NULL;

            optarg = NULL;

            if (next == NULL || *next == char('\0'))
            {
                if (optind == 0)
                    optind++;

                if (optind >= argc || argv[optind][0] != char('-') || argv[optind][1] == char('\0'))
                {
                    optarg = NULL;
                    if (optind < argc)
                        optarg = argv[optind];
                    return EOF;
                }

                if (strcmp(argv[optind], "--") == 0)
                {
                    optind++;
                    optarg = NULL;
                    if (optind < argc)
                        optarg = argv[optind];
                    return EOF;
                }

                next = argv[optind];
                next++;		// skip past -
                optind++;
            }

            char c = *next++;
            char *cp = strchr(optstring, c);

            if (cp == NULL || c == char(':'))
                return char('?');

            cp++;
            if (*cp == char(':'))
            {
                if (*next != char('\0'))
                {
                    optarg = next;
                    next = NULL;
                }
                else if (optind < argc)
                {
                    optarg = argv[optind];
                    optind++;
                }
                else
                {
                    return char('?');
                }
            }

            return c;
        }
    }

    int log_level = Debug;  // default
    int log_level_cout = Debug;  // default
    std::string log_file;
    std::ofstream log_out;

    c_log::c_log(const std::string & prefix) : _prefix(prefix)
    {
    }

    c_log::~c_log();
    c_log & c_log::operator()(int level, const std::string & msg)
    {
        if (!log_out)
            std::cout << "log file output is not ready!\n";
        else if (level <= log_level)
            log_out << msg;

        if (level <= log_level_cout)    std::cout << msg;
        return (*this);
    }

    c_log & c_log::operator()(int level, const char * fmt, ...)
    {
        va_list marker;
        va_start(marker, fmt);
        int size = 10240;
        std::vector<char> buf(size);
        if (vsprintf(&buf[0], fmt, marker) >= size)
            throw exception("c_log, format string large than maximum size");
        return (*this)(level, std::string(&buf[0]));
    }

    c_log & c_log::operator()(const char * fmt, ...)
    {
        va_list marker;
        va_start(marker, fmt);
        int size = 10240;
        std::vector<char> buf(size);
        if (vsprintf(&buf[0], fmt, marker) >= size)
            throw exception("c_log, format string large than maximum size");
        return (*this)(std::string(&buf[0]));
    }

    // implement c_log
    c_log & c_log::get_log()
    {
        static c_log _log;
        return _log;
    }

    std::string format(const char * fmt, ...)
    {
        va_list marker;
        va_start(marker, fmt);
        int size = 10240;
        std::vector<char> buf(size);
        if (vsprintf(&buf[0], fmt, marker) >= size)
            throw exception("format string large than maximum size");
        return std::string(&buf[0]);
    }

    void test_opts() 
    {
        opt::reset();

        int argc = 8;
        char * argv[] = { "test_case", "-ab", "-c" , "-C", "-d", "foo", "-e123", "xyz" };

        std::cout << "command line : ";
        for (int i=0; i<argc; ++i)
            std::cout << argv[i] << (i==argc-1?"":" ");
        std::cout << "\n";

        int c;
        while ((c = opt::getopt(argc, argv, "abcCd:e:f")) != EOF)
        {
            switch (c)
            {
            case 'a':
            case 'b':
            case 'c':
            case 'C':
                std::cout << "\toption " << (char)c << "\n";
                break;
            case 'd':
                std::cout << "\toption d with value " << opt::optarg << "\n";
                break;
            case 'e':
                std::cout << "\toption e with value " << opt::optarg << "\n";
                break;
            case '?':           // illegal option - i.e., an option that was
                // not specified in the optstring
                // Note: you may choose to ignore illegal options
                std::cout << "\tERROR:  illegal option " << argv[opt::optind-1] << "\n";
                std::cout << "Usage: cmd -a -b -c -C -d AAA -e NNN -f\n";
                break;

            default:                // legal option - it was specified in optstring,
                // but it had no "case" handler
                // Note: you may choose not to make this an error
                std::cout << "\tWARNING:  no handler for option " << c << "\n";
                break;
            }
        }

        if (opt::optind < argc)
        {
            std::string strArgs;
            strArgs = "";

            // In this loop you would save any extra arguments (e.g., filenames).
            while (opt::optind < argc)
            {
                if (strArgs.empty())
                    strArgs = "\tAdditional non-option arguments: ";
                strArgs += "<";
                strArgs += argv[opt::optind];
                strArgs += "> ";
                opt::optind++;
            }

            if (!strArgs.empty())
                std::cout << strArgs << "\n";
        }
    }

    class c_obj {
    public:
        int i;
        c_obj(int j) : i(j) { std::cout << "c_obj(int)\n"; }
        c_obj() : i(0) { std::cout << "c_obj()\n"; }
        ~c_obj() { std::cout << "~c_obj()\n"; }
    };

    void test_misc_func(const c_obj & o)
    {
        std::cout << "test_misc_func, c_obj address : " << &o << ", " << o.i << "\n";
    }

    void test_misc() 
    {
        test_misc_func(1);
        c_obj obj(2);
        test_misc_func(obj);
    }
}


