project "Networking"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "src/**.h",
        "src/**.cpp"
    }

    includedirs
    {
        "%{wks.location}/Renderer/src",
        "dependencies/asio-1.24.0/include",
        "src"
    }

    links
    {
        "Renderer"
    }

    filter "system:linux"
        postbuildcommands
        {
            ("cp -R -l Res \"%{wks.location}bin/" .. outputdir .. "/%{prj.name}/Res\"")
        }

    filter "system:windows"
		systemversion "latest"

        postbuildcommands
        { 
            "Xcopy /E /I /y Res \"%{wks.location}bin\\" .. outputdir .. "\\%{prj.name}\\Res\""
        }

	filter "configurations:Debug"
		defines "DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "RELEASE"
		runtime "Release"
		optimize "on"