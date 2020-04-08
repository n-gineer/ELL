#--------------------------------------------------------------------------
# Quickstart docker container for ELL (https://microsoft.github.io/ELL/)
# Ubuntu 18.04, Miniconda, Python 3.6
#--------------------------------------------------------------------------

FROM ell-dependencies

RUN conda create -n py36 numpy python=3.6
RUN activate py36

ENV ELL_ROOT "/ELL"

# Get the ELL source
ADD . ${ELL_ROOT}

WORKDIR /${ELL_ROOT}/build
RUN cmake ..
RUN make
RUN make _ELL_python

# Add ELL's build/bin folder to the PATH
ENV ELL_BUILD "/ELL/build/bin"
ENV PATH "$PATH:$ELL_BUILD"

# Install ONNX for import-onnx.py
RUN pip install onnx

# Install helpers for running command line scripts
RUN pip install \
    argparse \
    validators

# Create a "/model" directory as a workspace for custom jobs
WORKDIR /model

ENTRYPOINT ["/bin/bash"]