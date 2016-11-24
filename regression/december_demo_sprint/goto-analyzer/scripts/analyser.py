import os
import shutil


def __get_my_dir(): return os.path.dirname(os.path.realpath(__file__))


def get_binary_dir():
    return os.path.abspath(__get_my_dir() + "/../../../../src/goto-analyzer")


def get_binary_file():
    return os.path.abspath(get_binary_dir() + "/goto-analyzer")


def __get_make_dir():
    return os.path.abspath(get_binary_dir() + "/..")


def exists_java_script():
    return os.path.exists(os.path.join(__get_my_dir(),"__diffblue_full_class_name_parser__.class"))


def build_java_script():
    old_cwd = os.getcwd()
    os.chdir(__get_my_dir())
    os.system("ant")
    os.chdir(old_cwd)


def exists_goto_analyser():
    return os.path.exists(get_binary_file())


def build_goto_analyser():
    old_current_dir = os.getcwd()
    os.chdir(__get_make_dir())
    os.system("make all")
    os.chdir(old_current_dir)


def find_jar_containing_root_function(root_fn, jars_cfg):
    print("Searching for JAR file containing root function: " + root_fn)
    last_dot_index = root_fn.rfind(".")
    if last_dot_index < 1:
        print("ERROR: Cannot extract class name from function name in the root function specifier: " + root_fn)
        return ""
    relative_class_file_name = root_fn[:last_dot_index].replace('.', '/') + ".class"
    for jar_pathname in jars_cfg.keys():
        classes_root_dir = jars_cfg[jar_pathname]
        if os.path.exists(os.path.join(classes_root_dir,relative_class_file_name)):
            return jar_pathname
    return ""


def run_goto_analyser(root_fn,root_jar,jars_cfg,taint_json_file,results_dir):
    print("Starting 'goto-analyser' for root function: " + root_fn)
    if not os.path.exists(results_dir):
        os.makedirs(results_dir)
    root_jar_copy = os.path.abspath(os.path.join(results_dir, os.path.basename(root_jar)))
    shutil.copyfile(root_jar, root_jar_copy)
    classpath = os.path.relpath(os.path.abspath(os.path.join(__get_my_dir(),"../data/openjdk-8-rt.jar-unpacked-PRUNED")),
                                results_dir)
    for jar in jars_cfg["jars"]:
        if not (jar == root_jar):
            classpath += ":" + os.path.relpath(jar, results_dir)
    if not os.path.exists(results_dir):
        os.makedirs(results_dir)
    old_cwd = os.getcwd()
    os.chdir(results_dir)
    command = (
        get_binary_file() + " "
        + "'./" + os.path.basename(root_jar_copy) + "' "
        "--function '" + root_fn + "' "
        "--taint '" + os.path.relpath(taint_json_file,results_dir) + "' "
        "--summary-only "
        "--taint-dump-html-full-summaries "
        "--taint-dump-html-statistics "
        "--taint-dump-html-traces "
        "--taint-summaries-timeout-seconds 300 "
        "--verbosity 9 "
        "--classpath '" + classpath + "'"
        )
    #print(command)
    os.system(command)
    os.chdir(old_cwd)
    #os.remove(root_jar_copy)
