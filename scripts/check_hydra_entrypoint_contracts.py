from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    install_sh = read("install.sh")
    cmake = read("CMakeLists.txt")

    require(
        'DEFAULT_HYDRA_SCENE_PATH="/home/yalu/docker/assets/unit_test/anim/pao/pao.usd"' in install_sh
        and install_sh.count('hydra_scene_path="${HYDRA_SCENE_PATH:-${DEFAULT_HYDRA_SCENE_PATH}}"') >= 2,
        "Hydra entrypoints must share a HYDRA_SCENE_PATH-backed default scene path",
    )
    require(
        '/home/yalu/software/usdtweak/build/usdtweak "${hydra_scene_path}"' in install_sh,
        "install.sh show must open the HYDRA_SCENE_PATH-backed scene path",
    )
    require(
        "contracts.hydra_entrypoint" in cmake,
        "CMake must register the Hydra entrypoint contract",
    )


if __name__ == "__main__":
    main()
