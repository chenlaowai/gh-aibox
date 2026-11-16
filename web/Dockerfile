FROM 10.10.140.21/basic/node:18.20.3 as node
WORKDIR /app

ARG VUE_ENV
RUN echo "env: $VUE_ENV"
RUN npm config set registry https://rep.ccgdc.com/repository/npm-group/
RUN npm cache clean --force
RUN npm install -g pnpm
COPY package.json .
RUN pnpm install
COPY / .

RUN pnpm build:$VUE_ENV

# 复制自定义的 Nginx 配置文件到容器中
COPY default.conf /etc/nginx/default.conf

FROM 10.10.140.21/basic/nginx:1.16.1-alpine as nginx
COPY --from=node /app/dist /usr/share/nginx/html/

# 拷贝自定义的 Nginx 配置文件到 Nginx 容器中
COPY --from=node /etc/nginx/default.conf /etc/nginx/conf.d/default.conf

EXPOSE 80
CMD ["nginx", "-g", "daemon off;"]

