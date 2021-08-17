cmake_minimum_required(VERSION 3.5)
project(RAC_API VERSION 1.0.0 LANGUAGES CSharp)

include(CSharpUtilities)

add_executable(RAC
    App.config
    App.xaml
    App.xaml.cs
    MainWindow.xaml
    MainWindow.xaml.cs

	Images/AppIcon.ico
	Images/buttongood.png
	Images/buttonbad.png
    Images/buttonnuetral.png

    Properties/AssemblyInfo.cs
    Properties/Resources.Designer.cs
    Properties/Resources.resx
    Properties/Settings.Designer.cs
    Properties/Settings.settings)
	
csharp_set_designer_cs_properties(
    Properties/AssemblyInfo.cs
    Properties/Resources.Designer.cs
    Properties/Resources.resx
    Properties/Settings.Designer.cs
    Properties/Settings.settings)

csharp_set_xaml_cs_properties(
    App.xaml
    App.xaml.cs
    MainWindow.xaml
    MainWindow.xaml.cs)


set_property(SOURCE App.xaml PROPERTY VS_XAML_TYPE "ApplicationDefinition")
set_property(SOURCE "Images/buttongood.png" PROPERTY VS_TOOL_OVERRIDE "Resource")
set_property(SOURCE "Images/buttonbad.png" PROPERTY VS_TOOL_OVERRIDE "Resource")
set_property(SOURCE "Images/buttonnuetral.png" PROPERTY VS_TOOL_OVERRIDE "Resource")
set_property(SOURCE "Images/AppIcon.ico" PROPERTY VS_TOOL_OVERRIDE "Resource")

set_property(TARGET RAC PROPERTY VS_DOTNET_REFERENCES
    "Microsoft.CSharp"
    "PresentationCore"
    "PresentationFramework"
    "System"
    "System.Memory"
    "System.Core"
    "System.Data"
    "System.Data.DataSetExtensions"
	"System.Drawing"
    "System.Xaml"
    "System.Xml"
    "System.Xml.Linq"
    "System.Text.Json"
    "System.Runtime.CompilerServices.Unsafe"
    "WindowsBase")
	
set_property(TARGET RAC PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2")
set_property(TARGET RAC PROPERTY WIN32_EXECUTABLE TRUE)
set_property(TARGET RAC PROPERTY FOLDER "APPLICATIONS")


add_dependencies( RAC simpleconnectioncoordinator remoteviewer )	