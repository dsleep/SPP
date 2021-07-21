cmake_minimum_required(VERSION 3.5)
project(AppNetTest_WPF VERSION 1.0.0 LANGUAGES CSharp)

include(CSharpUtilities)

add_executable(AppThroughput
    App.config
    App.xaml
    App.xaml.cs
    MainWindow.xaml
    MainWindow.xaml.cs
	
	Images/AppIcon.ico
	Images/buttongood.png
	Images/buttonbad.png

    Properties/AssemblyInfo.cs
    Properties/Resources.Designer.cs
    Properties/Resources.resx
    Properties/Settings.Designer.cs
    Properties/Settings.settings)

add_dependencies( AppThroughput applicationhost simpleconnectioncoordinator )	
	
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

#set_property(TARGET RAM
#    PROPERTY VS_PACKAGE_REFERENCES
#		"OxyPlot.Core_2.0.0"
#		"OxyPlot.Wpf_2.0.0"
#)

set_property(TARGET AppThroughput PROPERTY VS_DOTNET_REFERENCES
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
	

set_property(TARGET AppThroughput PROPERTY 
    VS_DOTNET_REFERENCE_OxyPlot.Core 
    ${THIRD_PARTY}/OxyPlot.Core.2.0.0/lib/net45/OxyPlot.dll
)

set_property(TARGET AppThroughput PROPERTY 
    VS_DOTNET_REFERENCE_OxyPlot.Wpf
    ${THIRD_PARTY}/OxyPlot.Wpf.2.0.0/lib/net45/OxyPlot.Wpf.dll
)


set_property(TARGET AppThroughput PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2")
set_property(TARGET AppThroughput PROPERTY WIN32_EXECUTABLE TRUE)
set_property(TARGET AppThroughput PROPERTY FOLDER "APPLICATIONS")