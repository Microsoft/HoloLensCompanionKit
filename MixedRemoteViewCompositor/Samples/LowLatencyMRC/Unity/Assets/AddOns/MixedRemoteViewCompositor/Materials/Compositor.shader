// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

Shader "Hidden/Compositor"
{
    Properties
    {
        _MainTex("Main Texture", 2D) = "" {}
        _VideoTex("Video Texture", 2D) = "black" {}
    }

    CGINCLUDE
    #include "UnityCG.cginc"

    struct v2f
    {
        float4 pos : SV_POSITION;
        float2 uv[2] : TEXCOORD0;
    };

    sampler2D _MainTex;
    sampler2D _VideoTex;

    half4 _MainTex_TexelSize;
    float _Transparency = 1.0f;
    float _Brightness = 1.0f;

    v2f vert(appdata_img v)
    {
        v2f o;
        o.pos = UnityObjectToClipPos(v.vertex);

        o.uv[0] = v.texcoord.xy;

#if UNITY_UV_STARTS_AT_TOP
        if (_MainTex_TexelSize.y < 0.0)
        {
            o.uv[0].y = 1.0 - o.uv[0].y;
        }
#endif

        o.uv[1] = v.texcoord.xy;

        return o;
    }

    float4 fragComposite(v2f i) : SV_Target
    {
        // get pixel for video and hologram
        float4 video = tex2D(_VideoTex, float2(i.uv[0].x, 1 - i.uv[0].y));
        float4 hologram = tex2D(_MainTex, i.uv[1]);

        // increase contrast and adjust brightness
        video = saturate(pow(video, .9f)) * _Brightness;

        // adjust hologram to look better
        hologram = float4(pow(hologram.rgb, 0.75f), hologram.a);

        // calc alpha to darken video in the correct place
        float greyScale = dot(hologram.rgb, float3(0.3f, 0.59f, 0.11f));

        float alpha = pow(greyScale, _Transparency);

        return lerp(video, hologram, alpha);

        return hologram;
    }
    ENDCG

    Subshader
    {
        ZTest Always Cull Off ZWrite Off

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment fragComposite
            ENDCG
        }
    }

    Fallback off
}