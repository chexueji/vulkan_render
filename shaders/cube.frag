#version 450

struct Material
{
    vec3 ambient;
    vec3 diffuse;
    vec3 emissive;
    float opacity;
    vec3 specular;
    float shininess;
    float metallic;
    float roughness;
};

layout(set = 0, binding = 5) uniform ucolor
{
    vec3 ambient;
    vec3 diffuse;
    vec3 emissive;
    float opacity;
    vec3 specular;
    float shininess;
    float metallic;
    float roughness;
} u_Color;

layout(set = 0, binding = 3) uniform LightColor
{
    vec3 u_lightColor[1];
} _125;

layout(location = 0) in vec3 v_vertexToEyeDirection;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec3 v_vertexToPointLightDirection[1];
//layout(location = 3) in vec3 in_color;
layout(location = 0) out vec4 out_color;

vec3 computePointLight(vec3 vertexToPointLightDirection, vec3 normal, vec3 viewDir, vec3 lightColor, vec3 color_diffuse, vec3 color_special)
{
    vec3 lightDir = normalize(vertexToPointLightDirection);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = lightColor * (color_diffuse * diff);
    float spec = pow(max(dot(normal, normalize(lightDir + viewDir)), 0.0), u_Color.shininess);
    vec3 specular = lightColor * (color_special * spec);
    return diffuse + specular;
}

void main()
{
    vec3 _69;
    if (gl_FrontFacing)
    {
        _69 = normalize(v_normal);
    }
    else
    {
        _69 = -normalize(v_normal);
    }
    vec3 norm = _69;
    vec3 viewDir = normalize(v_vertexToEyeDirection);
    vec3 color_ambient = u_Color.ambient;
    vec3 color_diffuse = u_Color.diffuse;
    vec3 color_special = u_Color.specular;
    vec3 color_emissive = u_Color.emissive;
    vec3 reflection = vec3(0.0);
    float alpha = u_Color.opacity;
    vec3 pointLightEffect = color_ambient * color_diffuse;
    vec3 param = v_vertexToPointLightDirection[0];
    vec3 param_1 = norm;
    vec3 param_2 = viewDir;
    vec3 param_3 = _125.u_lightColor[0];
    vec3 param_4 = color_diffuse;
    vec3 param_5 = color_special;
    pointLightEffect += computePointLight(param, param_1, param_2, param_3, param_4, param_5);
    vec3 result = (color_emissive + reflection) + pointLightEffect;
    result = result;
    result = clamp(result, vec3(0.0), vec3(1.0));
    out_color = vec4(result, alpha);
}