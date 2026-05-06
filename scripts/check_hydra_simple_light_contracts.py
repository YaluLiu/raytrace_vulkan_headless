from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_all(text: str, needles: list[str], message: str) -> None:
    missing = [needle for needle in needles if needle not in text]
    require(not missing, f"{message}: missing {', '.join(missing)}")


def main() -> None:
    delegate_cpp = read("hdRobot/renderDelegate.cpp")
    light_h = read("hdRobot/light.h")
    light_cpp = read("hdRobot/light.cpp")
    cmake = read("CMakeLists.txt")

    require(
        "HdPrimTypeTokens->simpleLight" in delegate_cpp,
        "Hydra render delegate must advertise simpleLight support",
    )
    require(
        "new HdRobotSimpleLight" in delegate_cpp,
        "Hydra render delegate must construct HdRobotSimpleLight for simpleLight sprims",
    )
    require("class HdRobotSimpleLight" in light_h, "Simple light sprim class must be declared")
    require_all(
        light_cpp,
        [
            "HdRobotSimpleLight::HdRobotSimpleLight",
            "HdRobotSimpleLight::Sync",
            "HdLightTokens->params",
            "IsHolding<GlfSimpleLight>",
            "Get<GlfSimpleLight>",
            "GetPosition()",
            "GetDiffuse()",
            "GetSpecular()",
            "type = 0",
            "SimpleLight::Sync",
        ],
        "Simple light sync must read GlfSimpleLight params and map them to renderer sphere light fields",
    )
    require("contracts.hydra_simple_light" in cmake, "CMake must register the Hydra simple light contract")


if __name__ == "__main__":
    main()
