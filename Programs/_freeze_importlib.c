/* This is built as a stand-alone executable by the Makefile, and helps turn
   Lib/importlib/_bootstrap.py into a frozen module in Python/importlib.h
*/

#include <Python.h>
#include <marshal.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MS_WINDOWS
#include <unistd.h>
#endif

/* To avoid a circular dependency on frozen.o, we create our own structure
   of frozen modules instead, left deliberately blank so as to avoid
   unintentional import of a stale version of _frozen_importlib. */

static const struct _frozen _PyImport_FrozenModules[] = {
    {0, 0, 0} /* sentinel */
};

#ifndef MS_WINDOWS
/* On Windows, this links with the regular pythonXY.dll, so this variable comes
   from frozen.obj. In the Makefile, frozen.o is not linked into this executable,
   so we define the variable here. */
const struct _frozen *PyImport_FrozenModules;
#endif

const char header[] = "/* Auto-generated by Programs/_freeze_importlib.c */";

int
main(int argc, char *argv[])
{
    char *inpath, *outpath, *code_name;
    FILE *infile = NULL, *outfile = NULL;
    struct _Py_stat_struct status;
    size_t text_size, data_size, n;
    char *text = NULL;
    unsigned char *data;
    PyObject *code = NULL, *marshalled = NULL;
    int is_bootstrap = 1;

    PyImport_FrozenModules = _PyImport_FrozenModules;

    if (argc != 3) {
        fprintf(stderr, "need to specify input and output paths\n");
        return 2;
    }
    inpath = argv[1];
    outpath = argv[2];
    infile = fopen(inpath, "rb");
    if (infile == NULL) {
        fprintf(stderr, "cannot open '%s' for reading\n", inpath);
        goto error;
    }
    if (_Py_fstat_noraise(fileno(infile), &status)) {
        fprintf(stderr, "cannot fstat '%s'\n", inpath);
        goto error;
    }
    text_size = (size_t)status.st_size;
    text = (char *) malloc(text_size + 1);
    if (text == NULL) {
        fprintf(stderr, "could not allocate %ld bytes\n", (long) text_size);
        goto error;
    }
    n = fread(text, 1, text_size, infile);
    fclose(infile);
    infile = NULL;
    if (n < text_size) {
        fprintf(stderr, "read too short: got %ld instead of %ld bytes\n",
                (long) n, (long) text_size);
        goto error;
    }
    text[text_size] = '\0';

    _PyCoreConfig config = _PyCoreConfig_INIT;
    config.user_site_directory = 0;
    config.site_import = 0;
    config.use_environment = 0;
    config.program_name = L"./_freeze_importlib";
    /* Don't install importlib, since it could execute outdated bytecode. */
    config._install_importlib = 0;
    config.install_signal_handlers = 1;
    config._frozen = 1;

    _PyInitError err = _Py_InitializeFromConfig(&config);
    /* No need to call _PyCoreConfig_Clear() since we didn't allocate any
       memory: program_name is a constant string. */
    if (_Py_INIT_FAILED(err)) {
        _Py_FatalInitError(err);
    }

    if (strstr(inpath, "_external") != NULL) {
        is_bootstrap = 0;
    }

    code_name = is_bootstrap ?
        "<frozen importlib._bootstrap>" :
        "<frozen importlib._bootstrap_external>";
    code = Py_CompileStringExFlags(text, code_name, Py_file_input, NULL, 0);
    if (code == NULL)
        goto error;
    free(text);
    text = NULL;

    marshalled = PyMarshal_WriteObjectToString(code, Py_MARSHAL_VERSION);
    Py_CLEAR(code);
    if (marshalled == NULL)
        goto error;

    assert(PyBytes_CheckExact(marshalled));
    data = (unsigned char *) PyBytes_AS_STRING(marshalled);
    data_size = PyBytes_GET_SIZE(marshalled);

    /* Open the file in text mode. The hg checkout should be using the eol extension,
       which in turn should cause the EOL style match the C library's text mode */
    outfile = fopen(outpath, "w");
    if (outfile == NULL) {
        fprintf(stderr, "cannot open '%s' for writing\n", outpath);
        goto error;
    }
    fprintf(outfile, "%s\n", header);
    if (is_bootstrap)
        fprintf(outfile, "const unsigned char _Py_M__importlib[] = {\n");
    else
        fprintf(outfile,
                "const unsigned char _Py_M__importlib_external[] = {\n");
    for (n = 0; n < data_size; n += 16) {
        size_t i, end = Py_MIN(n + 16, data_size);
        fprintf(outfile, "    ");
        for (i = n; i < end; i++) {
            fprintf(outfile, "%d,", (unsigned int) data[i]);
        }
        fprintf(outfile, "\n");
    }
    fprintf(outfile, "};\n");

    Py_CLEAR(marshalled);

    Py_Finalize();
    if (outfile) {
        if (ferror(outfile)) {
            fprintf(stderr, "error when writing to '%s'\n", outpath);
            goto error;
        }
        fclose(outfile);
    }
    return 0;

error:
    PyErr_Print();
    Py_Finalize();
    if (infile)
        fclose(infile);
    if (outfile)
        fclose(outfile);
    if (text)
        free(text);
    if (marshalled)
        Py_DECREF(marshalled);
    return 1;
}
