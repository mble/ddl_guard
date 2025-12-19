ARG POSTGRES_VERSION=16
FROM postgres:${POSTGRES_VERSION}-bookworm

ARG POSTGRES_VERSION=16
ENV POSTGRES_VERSION=${POSTGRES_VERSION}

RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		build-essential \
		postgresql-server-dev-${POSTGRES_VERSION} \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY docker/run-tests.sh /usr/local/bin/ddl-guard-test
RUN chmod +x /usr/local/bin/ddl-guard-test

COPY . /workspace

ENTRYPOINT ["/usr/local/bin/ddl-guard-test"]
