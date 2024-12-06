#define KNOB_IMPLEMENTATION
#include "knob.h"

static const char *raylib_modules[] = {
    "rcore",
    "raudio",
    "rglfw",
    "rmodels",
    "rshapes",
    "rtext",
    "rtextures",
    "utils",
};

#define RAYLIB_PATH "Libraries/raylib"
#define RAYLIB_BUILD_PATH "build/raylib"
bool build_raylib(Knob_File_Paths* end_cmd,Knob_Config* config)
{
    bool result = true;
    Knob_Cmd cmd = {0};

    Knob_Procs procs = {0};


    if (!knob_mkdir_if_not_exists(RAYLIB_BUILD_PATH)) {
        knob_return_defer(false);
    }

    for (size_t i = 0; i < KNOB_ARRAY_LEN(raylib_modules); ++i) {
        const char *input_path = knob_temp_sprintf(RAYLIB_PATH"/src/%s.c", raylib_modules[i]);
        const char *output_path = knob_temp_sprintf("%s/%s.o", RAYLIB_BUILD_PATH, raylib_modules[i]);
        output_path = knob_temp_sprintf("%s/%s.o", RAYLIB_BUILD_PATH, raylib_modules[i]);
        knob_da_append(end_cmd,output_path);

        if (knob_needs_rebuild(output_path, &input_path, 1)) {
            cmd.count = 0;
            knob_cmd_append(&cmd, GET_COMPILER_NAME(config->compiler));
            knob_cmd_append(&cmd, "-DPLATFORM_DESKTOP","-fPIC","-DSUPPORT_FILEFORMAT_BMP=1","-DSUPPORT_FILEFORMAT_JPG=1");
            #ifdef __linux__
            knob_cmd_append(&cmd, "-D_GLFW_X11");
            #endif
            knob_cmd_append(&cmd, "-ggdb3");
            knob_cmd_append(&cmd, "-I"RAYLIB_PATH"/src/external/glfw/include");
            knob_cmd_append(&cmd, "-I"RAYLIB_PATH"/src");
            knob_cmd_append(&cmd, "-c", input_path);
            knob_cmd_append(&cmd, "-o", output_path); 

            Knob_Proc proc = knob_cmd_run_async(cmd,NULL,NULL);
            knob_da_append(&procs, proc);
        }
    }
    cmd.count = 0;

    if (!knob_procs_wait(procs)) knob_return_defer(false);

defer:
    knob_cmd_free(cmd);
    return result;
}

#define IMGUI_PATH "Libraries/imgui"
bool build_rlImGui(Knob_File_Paths* end_cmd,Knob_Config* config)
{
    bool result = true;
    Knob_Cmd cmd = {0};

    Knob_Config conf = {0};
    knob_config_init(&conf);
    conf.compiler = config->compiler;
    conf.debug_mode = config->debug_mode;

    Knob_File_Paths files = {0};
    knob_da_mult_append(&files,
        //UI
        IMGUI_PATH"/imgui.cpp",
        IMGUI_PATH"/imgui_draw.cpp",
        IMGUI_PATH"/imgui_tables.cpp",
        IMGUI_PATH"/imgui_widgets.cpp",
        IMGUI_PATH"/imgui_demo.cpp",
        //------------------------------
        "Libraries/rlImGui/rlImGui.cpp",
    );
    knob_config_add_files(&conf,files.items,files.count);
    files.count = 0;

    knob_da_mult_append(&files,
        RAYLIB_PATH"/src/external/glfw/include",
        RAYLIB_PATH"/src",
        IMGUI_PATH,
        "Libraries/rlImGui",
    );
    knob_config_add_includes(&conf,files.items,files.count);
    files.count = 0;

    knob_config_add_define(&conf,"-fPIC");

    conf.build_to = RAYLIB_BUILD_PATH;
    knob_config_build(&conf,&files,0);
    for(int i =0; i < files.count; ++i){
        const char* output_path = files.items[i];
        knob_da_append(end_cmd,output_path);
    }

defer:
    knob_cmd_free(cmd);
    return result;
}
#define NFD_PATH "Libraries/nativefiledialog-extended"
bool build_nfd(Knob_File_Paths* end_cmd,Knob_Config* config)
{
    bool result = true;
    Knob_Config conf = {0};
    knob_config_init(&conf);
    conf.compiler = config->compiler;
    conf.debug_mode = config->debug_mode;

#if defined(__WIN32)
    knob_config_add_cpp_flag(&conf,"-lole32");
    knob_config_add_cpp_flag(&conf,"-luuid");
    knob_config_add_cpp_flag(&conf,"-lshell32");
#elif defined(__APPLE__)
    knob_config_add_cpp_flag(&conf,"-framework AppKit");
    knob_config_add_cpp_flag(&conf,"-framework UniformTypeIdentifiers");
#else
    Knob_Cmd cmd = {0};
    Knob_Pipe pipe = knob_pipe_make();
    const char* arr_flags[] = {
        "--libs",
        "--cflags"
    };
    for(int i =0; i < KNOB_ARRAY_LEN(arr_flags);++i){
        cmd.count = 0;
        knob_cmd_append(&cmd,"pkg-config",arr_flags[i],"dbus-1");
        Knob_Proc p = knob_cmd_run_async(cmd,NULL,&pipe.write);
        if (p == KNOB_INVALID_PROC) return false;
        knob_proc_wait(p);
        char buff[1024] = {0};
        if(read(pipe.read,buff,1024) < 0){
            return false;
        }
        Knob_String_View sv = knob_sv_from_cstr(buff);
        while(sv.count > 0){
            Knob_String_View temp_sv = knob_sv_chop_by_delim(&sv,' ');
            if(sv.count == 0){
                temp_sv = knob_sv_chop_by_delim(&temp_sv,'\n');
            }
            const char* out = knob_temp_sv_to_cstr(temp_sv);
            if(i == 0){
                knob_config_add_c_flag(config,out);
                knob_config_add_cpp_flag(config,out);
            }
            knob_config_add_cpp_flag(&conf,out);
        }
    }
#endif

    Knob_File_Paths files = {0};
    knob_da_mult_append(&files,
        #if  defined(_WIN32)
        NFD_PATH"/src/nfd_win.cpp",
        #elif defined(__APPLE__)
        NFD_PATH"/src/nfd_cocoa.m",
        #else
        NFD_PATH"/src/nfd_portal.cpp",
        #endif
    );
    knob_config_add_files(&conf,files.items,files.count);
    files.count = 0;

    knob_da_mult_append(&files,
        NFD_PATH"/src/include",
    );
    knob_config_add_includes(&conf,files.items,files.count);
    files.count = 0;

    knob_config_add_define(&conf,"-fPIC");

    conf.build_to = RAYLIB_BUILD_PATH;
    knob_config_build(&conf,&files,1);
    for(int i =0; i < files.count; ++i){
        const char* output_path = files.items[i];
        knob_da_append(end_cmd,output_path);
    }

defer:
    knob_cmd_free(cmd);
    return result;
}
#define LUAU_D_BUILD_PATH "build/luaud"
#define CPP_DAP_PATH "Libraries/cppdap"
int build_cppdap(Knob_File_Paths* end_cmd,Knob_Config* config){
    
    bool result = true;
    Knob_Cmd cmd = {0};

    // @TODO: Add a check to update submodules if they aren't present in the cppdap lib
    // git submodule update --init

    Knob_Config conf = {0};
    knob_config_init(&conf);
    conf.compiler = config->compiler;
    conf.debug_mode = config->debug_mode;

    Knob_File_Paths files = {0};
    knob_da_mult_append(&files,
        CPP_DAP_PATH"/src/content_stream.cpp",
        CPP_DAP_PATH"/src/io.cpp",
        CPP_DAP_PATH"/src/jsoncpp_json_serializer.cpp",
        CPP_DAP_PATH"/src/network.cpp",
        CPP_DAP_PATH"/src/nlohmann_json_serializer.cpp",
        CPP_DAP_PATH"/src/null_json_serializer.cpp",
        CPP_DAP_PATH"/src/protocol_events.cpp",
        CPP_DAP_PATH"/src/protocol_requests.cpp",
        CPP_DAP_PATH"/src/protocol_response.cpp",
        CPP_DAP_PATH"/src/protocol_types.cpp",
        CPP_DAP_PATH"/src/session.cpp",
        CPP_DAP_PATH"/src/socket.cpp",
        CPP_DAP_PATH"/src/typeinfo.cpp",
        CPP_DAP_PATH"/src/typeof.cpp",
    );
    knob_config_add_files(&conf,files.items,files.count);
    files.count = 0;

    knob_da_mult_append(&files,
        CPP_DAP_PATH"/include",
        CPP_DAP_PATH"/src",
        CPP_DAP_PATH"/third_party/json/include",
    );
    knob_config_add_includes(&conf,files.items,files.count);
    files.count = 0;

    // knob_config_add_define(&conf,"-fPIC");
    knob_config_add_define(&conf,"-DCPPDAP_JSON_NLOHMANN");
    knob_config_add_define(&conf,"-std=c++17");

    conf.build_to = LUAU_D_BUILD_PATH;
    knob_config_build(&conf,&files,0);
    for(int i =0; i < files.count; ++i){
        const char* output_path = files.items[i];
        knob_da_append(end_cmd,output_path);
    }

defer:
    knob_cmd_free(cmd);
    return result;
}


#define LUAU_COMP_PATH "Libraries/luau"
int build_luau(Knob_File_Paths* end_cmd,Knob_Config* config){
    bool result = true;
    Knob_Cmd cmd = {0};

    Knob_Config conf = {0};
    knob_config_init(&conf);
    conf.compiler = config->compiler;
    conf.debug_mode = config->debug_mode;

    Knob_File_Paths files = {0};
    knob_da_mult_append(&files,
        LUAU_COMP_PATH"/VM/src",
        //Ast
        LUAU_COMP_PATH"/Ast/src/Allocator.cpp",
        LUAU_COMP_PATH"/Ast/src/Ast.cpp",
        LUAU_COMP_PATH"/Ast/src/Confusables.cpp",
        LUAU_COMP_PATH"/Ast/src/Lexer.cpp",
        LUAU_COMP_PATH"/Ast/src/Location.cpp",
        LUAU_COMP_PATH"/Ast/src/Parser.cpp",
        LUAU_COMP_PATH"/Ast/src/StringUtils.cpp",
        LUAU_COMP_PATH"/Ast/src/TimeTrace.cpp",
        //Compiler
        LUAU_COMP_PATH"/Compiler/src/BytecodeBuilder.cpp",
        LUAU_COMP_PATH"/Compiler/src/Compiler.cpp",
        LUAU_COMP_PATH"/Compiler/src/Types.cpp",
        LUAU_COMP_PATH"/Compiler/src/Builtins.cpp",
        LUAU_COMP_PATH"/Compiler/src/BuiltinFolding.cpp",
        LUAU_COMP_PATH"/Compiler/src/ConstantFolding.cpp",
        LUAU_COMP_PATH"/Compiler/src/CostModel.cpp",
        LUAU_COMP_PATH"/Compiler/src/TableShape.cpp",
        LUAU_COMP_PATH"/Compiler/src/ValueTracking.cpp",
        LUAU_COMP_PATH"/Compiler/src/lcode.cpp",
        //CLI, Probably won't needed in end app
        // LUAU_COMP_PATH"/CLI/Analyze.cpp",
        // LUAU_COMP_PATH"/CLI/Ast.cpp",
        // LUAU_COMP_PATH"/CLI/Bytecode.cpp",
        // LUAU_COMP_PATH"/CLI/Compile.cpp",
        // LUAU_COMP_PATH"/CLI/Coverage.cpp",
        LUAU_COMP_PATH"/CLI/FileUtils.cpp",
        LUAU_COMP_PATH"/CLI/Flags.cpp",
        // LUAU_COMP_PATH"/CLI/Profiler.cpp",
        // LUAU_COMP_PATH"/CLI/Reduce.cpp",
        // LUAU_COMP_PATH"/CLI/Repl.cpp",
        // LUAU_COMP_PATH"/CLI/ReplEntry.cpp",
        LUAU_COMP_PATH"/CLI/Require.cpp",
        // LUAU_COMP_PATH"/CLI/Web.cpp",
        LUAU_COMP_PATH"/Config/src/Config.cpp",
        LUAU_COMP_PATH"/Config/src/LinterConfig.cpp",
    );
    knob_config_add_files(&conf,files.items,files.count);
    files.count = 0;

    knob_da_mult_append(&files,
        LUAU_COMP_PATH"/Common/include",
        LUAU_COMP_PATH"/VM/include",
        LUAU_COMP_PATH"/VM/src",
        LUAU_COMP_PATH"/Ast/include",
        LUAU_COMP_PATH"/Compiler/include",
        LUAU_COMP_PATH"/Compiler/src",
        LUAU_COMP_PATH"/CLI",
        LUAU_COMP_PATH"/Config/include",
    );

    knob_config_add_includes(&conf,files.items,files.count);
    files.count = 0;
    
    knob_config_add_define(&conf,"-std=c++17");
    
    conf.build_to = LUAU_D_BUILD_PATH;
    knob_config_build(&conf,&files,0);
    for(int i =0; i < files.count; ++i){
        const char* output_path = files.items[i];
        knob_da_append(end_cmd,output_path);
    }

defer:
    knob_cmd_free(cmd);
    return result;
}

#define LUAU_D_PATH "Libraries/luau-debugger/debugger"
int build_luau_debugger(Knob_File_Paths* end_cmd,Knob_Config* config){
    
    bool result = true;
    if (!knob_mkdir_if_not_exists(LUAU_D_BUILD_PATH)) {
        knob_return_defer(false);
    }
    if(!build_cppdap(end_cmd,config)){
        knob_return_defer(false);
    }
    if(!build_luau(end_cmd,config)){
        knob_return_defer(false);
    }
    Knob_Cmd cmd = {0};


    Knob_Config conf = {0};
    knob_config_init(&conf);
    conf.compiler = config->compiler;
    conf.debug_mode = config->debug_mode;

    Knob_File_Paths files = {0};
    knob_da_mult_append(&files,
        LUAU_D_PATH"/src/debugger.cpp",
        LUAU_D_PATH"/src/internal/breakpoint.cpp",
        LUAU_D_PATH"/src/internal/file.cpp",
        LUAU_D_PATH"/src/internal/debug_bridge.cpp",
        LUAU_D_PATH"/src/internal/lua_statics.cpp",
        LUAU_D_PATH"/src/internal/variable.cpp",
        LUAU_D_PATH"/src/internal/variable_registry.cpp",
        LUAU_D_PATH"/src/internal/vm_registry.cpp",
        LUAU_D_PATH"/src/internal/utils/lua_utils.cpp",
        LUAU_D_PATH"/src/internal/utils/lua_types.cpp",
    );
    knob_config_add_files(&conf,files.items,files.count);
    files.count = 0;

    knob_da_mult_append(&files,
        LUAU_D_PATH"/src",
        LUAU_D_PATH"/include",
        CPP_DAP_PATH"/include",
        CPP_DAP_PATH"/src",
        CPP_DAP_PATH"/third_party/json/include",
        LUAU_COMP_PATH"/Common/include",
        LUAU_COMP_PATH"/VM/include",
        LUAU_COMP_PATH"/VM/src",
        LUAU_COMP_PATH"/Ast/include",
        LUAU_COMP_PATH"/Compiler/include",
    );
    knob_config_add_includes(&conf,files.items,files.count);
    files.count = 0;

    // knob_config_add_define(&conf,"-fPIC");
    knob_config_add_define(&conf,"-std=c++20");

    conf.build_to = LUAU_D_BUILD_PATH;
    knob_config_build(&conf,&files,0);
    for(int i =0; i < files.count; ++i){
        const char* output_path = files.items[i];
        knob_da_append(end_cmd,output_path);
    }

defer:
    knob_cmd_free(cmd);
    return result;
}

typedef enum {
    KNOB_FORMAT_LLVM,
    KNOB_FORMAT_GNU,
    KNOB_FORMAT_GOOGLE,
    KNOB_FORMAT_CHROMIUM,
    KNOB_FORMAT_MICROSOFT,
    KNOB_FORMAT_MOZILLA,
    KNOB_FORMAT_WEBKIT,
    KNOB_FORMAT_CUSTOM,
    KNOB_FORMAT_MAX
} Knob_Format_Style;

#define KNOB_CLANG_FORMAT_PATH "./Tools/clang-format.com"
int knob_format_code(Knob_File_Paths* filepaths,Knob_Format_Style format){
    static const char* format_strs[KNOB_FORMAT_MAX] = {
        "LLVM",
        "GNU",
        "Google",
        "Chromium",
        "Microsoft",
        "Mozilla",
        "WebKit",
        "file"
    };
    char* format_style = knob_temp_sprintf("--style=%s",format_strs[format]);
    bool result = true;
    Knob_Cmd cmd = {0};
    Knob_Procs procs = {0};

    for(int i =0; i < filepaths->count;++i){
        cmd.count = 0;
        char* path = (char*)filepaths->items[i]; 
        if(knob_path_is_dir(path)){
            Knob_File_Paths childs = {0};
            knob_read_entire_dir(path,&childs);
            for(int y =0 ; y < childs.count;++y){
                char* filename = (char*)childs.items[y];  
                if(knob_cstr_match(filename,"..") || knob_cstr_match(filename,".")) continue;
                const char* fname = knob_temp_sprintf("%s"PATH_SEP"%s",path,filename);
                if(!knob_format_code(&(Knob_File_Paths){ .items = &fname, .count= 1},format)){
                    knob_log(KNOB_ERROR,"Failed formating file: %s",fname);
                    knob_return_defer(false);
                }
            }
            knob_da_free(childs);
        }
        else {
            knob_cmd_append(&cmd,KNOB_CLANG_FORMAT_PATH,format_style,"-i",path);
            Knob_Proc proc = knob_cmd_run_async(cmd,NULL,NULL);
            knob_da_append(&procs, proc);
        }
    }
    if (!knob_procs_wait(procs)) knob_return_defer(false);
defer:
    knob_cmd_free(cmd);
    return result;
}

MAIN(FLED){
    
    if(!knob_mkdir_if_not_exists("build")){ return 1;}
    if(!knob_mkdir_if_not_exists("Deployment")){ return 1;}

    if(knob_file_exists("Deployment/imgui.ini")){
        knob_file_del("Deployment/imgui.ini");
    }

    const char* program = knob_shift_args(&argc,&argv);
    char* subcommand = NULL;
    if(argc <= 0){
        subcommand = "build";
    } else {
        subcommand = (char*)knob_shift_args(&argc,&argv);
    }
    if(knob_cstr_match(subcommand,"clean")){
        // knob_clean_dir("."PATH_SEP"build");
    }
    if(knob_cstr_match(subcommand,"build")){

    }

    Knob_Config config = {0};
    knob_config_init(&config);

    Knob_File_Paths o_files = {0};
    Knob_Cmd cmd = {0};
    // if(!build_raylib(&o_files,&config))return 1;
    // if(!build_rlImGui(&o_files,&config))return 1;
    // if(!build_nfd(&o_files,&config))return 1;
    if(!build_luau_debugger(&o_files,&config)) return 1;

    Knob_File_Paths files = {0};
    knob_da_mult_append(&files,
        "src",
    );

    knob_format_code(&files,KNOB_FORMAT_CUSTOM);

    config.build_to = "."PATH_SEP"build";
    knob_config_add_files(&config,files.items,files.count);
    files.count = 0;
    knob_da_mult_append(&files,
        ".",
        "./src",
        // LUAU_D_PATH"/src",
        LUAU_D_PATH"/include",
        LUAU_COMP_PATH"/Common/include",
        LUAU_COMP_PATH"/VM/include",
        LUAU_COMP_PATH"/VM/src",
        LUAU_COMP_PATH"/Ast/include",
        LUAU_COMP_PATH"/Compiler/include",
        LUAU_COMP_PATH"/CLI",
        LUAU_COMP_PATH"/Config/include",
        // "./Libraries/raylib/src",
        // "./Libraries/imgui",
        // "./Libraries/rlImGui",
        // NFD_PATH"/src/include"
    );
    knob_config_add_includes(&config,files.items,files.count);
    knob_config_add_define(&config,"-std=c++20");
    Knob_File_Paths out_files = {0};
    if(!knob_config_build(&config,&out_files,0))return 1;

    cmd.count = 0;

    knob_cmd_add_compiler(&cmd,&config);
    knob_cmd_add_includes(&cmd,&config);
    for(int i =0; i < o_files.count;++i){
        knob_cmd_append(&cmd,o_files.items[i]);
    }
    for(int i =0; i < out_files.count;++i){
        knob_cmd_append(&cmd,out_files.items[i]);
    }

    knob_cmd_append(&cmd,"-o","./Deployment/FLED.com");
    for(int i =0; i < config.cpp_flags.count;++i){
        knob_cmd_append(&cmd,config.cpp_flags.items[i]);
    }
    knob_cmd_append(&cmd,"-lm");
    knob_cmd_append(&cmd,"-lstdc++");
    knob_cmd_append(&cmd,"-ljsoncpp");

    Knob_String_Builder render = {0};
    knob_cmd_render(cmd,&render);
    knob_log(KNOB_INFO,"CMD: %s",render.items);
    if(!knob_cmd_run_sync(cmd)) return 1;

    return 0;
}
