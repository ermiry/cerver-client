FROM gcc as builder

WORKDIR /opt/client

# client
COPY . .
RUN make -j4 && make examples

############
FROM ubuntu:bionic

# install client
COPY --from=builder /opt/client/bin/libclient.so /usr/local/lib/
COPY --from=builder /opt/client/include/client /usr/local/include/client

RUN ldconfig

# client examples
WORKDIR /home/client
COPY --from=builder /opt/client/examples/bin ./examples

CMD ["./examples/test"]