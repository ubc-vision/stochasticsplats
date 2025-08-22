# StochasticSplats

Welcome to the StochasticSplats project repository!

<div align="center">
  <h1><b>StochasticSplats</b>: Stochastic Rasterization for Sorting-Free 3D Gaussian Splatting</h1>
  
  <p><strong>Accepted to ICCV 2025</strong></p>
  
  <p>
      <a href="https://ubc-vision.github.io/stochasticsplats/"><img src="https://img.shields.io/badge/Project-Page-blue?style=for-the-badge" alt="Project Page"></a>
      <a href="https://arxiv.org/abs/2503.24366"><img src="https://img.shields.io/badge/arXiv-2503.24366-b31b1b?style=for-the-badge" alt="arXiv"></a>
  </p>

  <p>
    <a href="https://shakibakh.github.io/">Shakiba Kheradmand</a><sup>1,2,*</sup>,
    <a href="https://dvicini.github.io/">Delio Vicini</a><sup>3,*</sup>,
    <a href="https://grgkopanas.github.io/">George Kopanas</a><sup>3,4,†</sup>,
    <a href="https://scholar.google.com/citations?user=sY8lt7AAAAAJ&hl=en">Dmitry Lagun</a><sup>1</sup>,
    <a href="https://www.cs.ubc.ca/~kmyi/">Kwang Moo Yi</a><sup>2</sup>,
    <a href="https://scholar.google.com/citations?user=glZNrscAAAAJ&hl=en">Mark Matthews</a><sup>1</sup>,
    <a href="https://theialab.ca/">Andrea Tagliasacchi</a><sup>1,5,6</sup>
  </p>

  <p>
    <sup>1</sup>Google DeepMind, <sup>2</sup>University of British Columbia, <sup>3</sup>Google, <sup>4</sup>Runway ML, <sup>5</sup>Simon Fraser University, <sup>6</sup>University of Toronto
  </p>
</div>


## Overview

This repository builds upon and extends the capabilities of [Splatapult](https://github.com/hyperlogic/splatapult), a 3D Gaussian splats viewer written by Anthony J. Thibault. StochasticSplats introduces stochastic rasterization methods enabling sorting-free 3D Gaussian splatting. You can run the app on desktop or in VR mode.

For a detailed explanation of controls and features, please refer to the [original README](https://github.com/hyperlogic/splatapult#readme).


## 🤝 Contributing  
Found a bug or have an idea to improve this project?  
I’d love your input! Please open a pull request.  
Thank you for helping make this project better.


## 🚀 How to Run

To run StochasticSplats, follow these steps:

### 1. Prepare Your Scene
Ensure your scene is in the standard 3D Gaussian Splatting format, which consists of a directory containing:
- `cameras.json`
- `input.ply`
- A `pointcloud/` subdirectory with the point cloud data.

### 2. Run the Viewer
Execute the viewer from your build directory, providing the path to your scene.

**Command Syntax:**
```sh
splatapult.exe [options] [path/to/scene] [options]
./build/splatapult [path/to/scene] [options]
```

**Example on Desktop:**
```sh
splatapult.exe path/to/my/scene --render_mode [AB | ST | ST-popfree] --width 1920 --height 1080
```

**Example on VR:**
```sh
splatapult.exe -v path/to/my/scene --render_mode [AB | ST | ST-popfree] --width 1692 --height 1824
```

### Command-Line Options
The following table details the command-line arguments added in this code base. For the complete options, please refere to [original README](https://github.com/hyperlogic/splatapult#readme).

| Option          | Description                                                                                                                                                                                       | Default |
|-----------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-------:|
| `--width`       | Sets the width of the application window.                                                                                                                                                         | `1296`  |
| `--height`      | Sets the height of the application window.                                                                                                                                                        | `840`   |
| `--render_mode` | Specifies the rendering mode.<ul><li>`AB`: Alpha Blending</li><li>`ST`: Stochastic Rendering (for original 3DGS scenes)</li><li>`ST-popfree`: Pop-free Stochastic Rendering (for scenes trained/finetuned using our method)</li></ul> | `AB`    |
| `--samples`     | Defines the number of samples for stochastic modes. The maximum value depends on your hardware.                                                                                                   |  `1`    |
| `--no-taa`      | Disables Temporal Anti-Aliasing (TAA). By default, TAA is enabled but automatically turns off when samples > 1.                                                                                  | `false` |


## Citation

If you utilize StochasticSplats in your research, please cite our ICCV 2025 paper:

```bibtex
@inproceedings{kv2025stochasticsplats,
  title={StochasticSplats: Stochastic Rasterization for Sorting-Free 3D Gaussian Splatting},
  author={Kheradmand, Shakiba and Vicini, Delio and Kopanas, George and Lagun, Dmitry and Yi, Kwang Moo and Matthews, Mark and Tagliasacchi, Andrea},
  booktitle={Proceedings of the IEEE/CVF International Conference on Computer Vision (ICCV)},
  year={2025}
}

```
