from conan import ConanFile

class MyProjectConan(ConanFile):
    name = "sen_dcs_exporter"
    version = "0.1.0"
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "CMakeToolchain", "VirtualRunEnv"

    def requirements(self):
        self.requires("sen/[>=1.0]")
        self.requires("cli11/2.6.2")
        self.requires("ftxui/6.1.9")
        self.requires("nlohmann_json/3.12.0")
