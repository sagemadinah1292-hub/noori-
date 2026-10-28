Vendored MIT dependencies (source headers, not neural model weights):

- Signalsmith Stretch 1.3.2, commit 88061fd4eda56a2353c5d65c74aa3f4ccf76f9da.
  https://github.com/Signalsmith-Audio/signalsmith-stretch
  License: ../licenses/Signalsmith-Stretch-MIT.txt
- Signalsmith Linear, commit 7bb1c8f36794d6980080ce817c91f8da8fe56a66.
  https://github.com/Signalsmith-Audio/linear
  License: signalsmith-linear/LICENSE.txt

Headers are copied without algorithm modifications. Default portable FFT backend
is used. Optional platform headers are retained to preserve upstream includes.
