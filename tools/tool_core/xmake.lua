shared_module("SkrToolCore", "TOOL_CORE", engine_version)
    set_group("02.tools")
    add_files("src/**.cpp")
    public_dependency("SkrRT", engine_version)
    add_includedirs("include", {public = true})
    add_rules("c++.codegen", {
        files = {"include/**.h", "include/**.hpp"},
        rootdir = "include/SkrToolCore",
        api = "TOOL_CORE"
    })
    -- add_rules("c++.unity_build", {batchsize = default_unity_batch_size})