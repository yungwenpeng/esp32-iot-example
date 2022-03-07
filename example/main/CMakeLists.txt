set(COMPONENT_SRCS "app_main.c"
                   "wifi.c"
                   "cloud_thingsboard.c"
	)
set(COMPONENT_ADD_INCLUDEDIRS ".")
# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
set(COMPONENT_EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem
                             ${project_dir}/server_certs/tb_server_cert.pem
                             ${project_dir}/server_certs/tb_mqtt_server_cert.pem
                             ${project_dir}/server_certs/tb_mqtt_client_cert.pem
                             ${project_dir}/server_certs/tb_mqtt_client_key.pem
                             ${project_dir}/server_certs/github_server_cert.pem
)

register_component()
