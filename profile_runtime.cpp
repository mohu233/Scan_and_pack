#include "profile_runtime.h"

#include <QDir>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {
bool save(const QString &path, const QByteArray &data, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) *error = QString("%1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

QByteArray jsValue(const QJsonValue &value)
{
    QJsonArray wrapper{value};
    QByteArray json = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    return json.mid(1, json.size() - 2);
}
}

bool ProfileRuntime::writeExtension(const QString &environmentPath,
                                    const QJsonObject &config,
                                    QString *extensionPath,
                                    QString *error)
{
    const QString path = QDir(environmentPath).absoluteFilePath("runtime-extension");
    if (!QDir().mkpath(path)) {
        if (error) *error = QString("Cannot create %1").arg(path);
        return false;
    }

    const QByteArray manifest = R"JSON({
  "manifest_version": 3,
  "name": "ProfilePower Runtime",
  "version": "1.0.0",
  "permissions": ["cookies", "storage"],
  "host_permissions": ["<all_urls>"],
  "background": {"service_worker": "background.js"},
  "content_scripts": [{"matches": ["<all_urls>"], "js": ["settings.js", "inject.js"], "run_at": "document_start", "world": "MAIN", "all_frames": true}]
})JSON";

    QJsonObject runtime;
    runtime["seed"] = config["env_id"];
    runtime["userAgent"] = config["user_agent"];
    runtime["platform"] = config["navigator_platform"];
    runtime["browserEngine"] = config["browser_engine"];
    runtime["browserMajorVersion"] = config["browser_major_version"];
    runtime["uaChPlatform"] = config["ua_ch_platform"];
    runtime["uaChPlatformVersion"] = config["ua_ch_platform_version"];
    runtime["uaChArchitecture"] = config["ua_ch_architecture"];
    runtime["uaChBitness"] = config["ua_ch_bitness"];
    runtime["uaChMobile"] = config["ua_ch_mobile"];
    runtime["acceptLanguage"] = config["accept_language"];
    runtime["language"] = config["ui_language"];
    runtime["languages"] = config["languages"];
    runtime["timezone"] = config["timezone"];
    runtime["timezoneMode"] = config["timezone_mode"];
    runtime["resolution"] = config["resolution"];
    runtime["geolocationMode"] = config["geolocation_mode"];
    runtime["geolocationModeIndex"] = config["geolocation_mode_index"];
    runtime["latitude"] = config["latitude"];
    runtime["longitude"] = config["longitude"];
    runtime["hardwareConcurrency"] = config["hardware_concurrency"];
    runtime["deviceMemory"] = config["device_memory"];
    runtime["disableWebRtc"] = config["disable_webrtc"];
    runtime["webglMode"] = config["webgl_mode"];
    runtime["webglVendor"] = config["webgl_vendor"];
    runtime["webglRenderer"] = config["webgl_renderer"];
    runtime["hardwareNoise"] = config["hardware_noise"];
    runtime["customFonts"] = config["custom_fonts"];
    const QByteArray settings = "globalThis.__PROFILE_POWER__=" + jsValue(runtime) + ";\n";

    const QByteArray inject = R"JS((() => {
'use strict';
const cfg=globalThis.__PROFILE_POWER__||{};try{delete globalThis.__PROFILE_POWER__}catch(_){}
const noise=cfg.hardwareNoise||{};let state=2166136261;for(const ch of String(cfg.seed||'profile')){state^=ch.charCodeAt(0);state=Math.imul(state,16777619)>>>0}
const delta=(salt=0)=>(((state^salt)%2001)-1000)/10000000;
const define=(object,key,value)=>{if(value===undefined||value===null||value==='')return;try{Object.defineProperty(object,key,{get:()=>value,configurable:true})}catch(_){}};
define(Navigator.prototype,'userAgent',cfg.userAgent);define(Navigator.prototype,'platform',cfg.platform);define(Navigator.prototype,'language',cfg.language);
define(Navigator.prototype,'languages',Array.isArray(cfg.languages)?Object.freeze(cfg.languages.slice()):undefined);
define(Navigator.prototype,'hardwareConcurrency',Number(cfg.hardwareConcurrency)||undefined);define(Navigator.prototype,'deviceMemory',Number(cfg.deviceMemory)||undefined);
if(cfg.userAgent&&navigator.userAgentData){const major=String(cfg.browserMajorVersion||'146'),engine=String(cfg.browserEngine||'Chrome');const product=engine==='Edge'?'Microsoft Edge':'Google Chrome';const brands=Object.freeze([{brand:'Not_A Brand',version:'24'},{brand:'Chromium',version:major},{brand:product,version:major}].map(Object.freeze));const fullVersionList=Object.freeze([{brand:'Not_A Brand',version:'24.0.0.0'},{brand:'Chromium',version:`${major}.0.0.0`},{brand:product,version:`${major}.0.0.0`}].map(Object.freeze));const base={brands,mobile:Boolean(cfg.uaChMobile),platform:String(cfg.uaChPlatform||'Windows')};const high={architecture:String(cfg.uaChArchitecture||'x86'),bitness:String(cfg.uaChBitness||'64'),formFactors:['Desktop'],fullVersionList,model:'',platformVersion:String(cfg.uaChPlatformVersion||''),uaFullVersion:`${major}.0.0.0`,wow64:false};const native=navigator.userAgentData,data=Object.create(Object.getPrototypeOf(native));Object.defineProperties(data,{brands:{get:()=>base.brands},mobile:{get:()=>base.mobile},platform:{get:()=>base.platform},getHighEntropyValues:{value:async hints=>{const result={...base};for(const hint of hints||[])if(Object.prototype.hasOwnProperty.call(high,hint))result[hint]=high[hint];return result}},toJSON:{value:()=>({...base})}});define(Navigator.prototype,'userAgentData',data)}
const size=String(cfg.resolution||'').match(/^(\d+)\s*[,xX]\s*(\d+)$/);if(size){const w=Number(size[1]),h=Number(size[2]);define(Screen.prototype,'width',w);define(Screen.prototype,'height',h);define(Screen.prototype,'availWidth',w);define(Screen.prototype,'availHeight',Math.max(0,h-40))}
if(cfg.timezone&&!String(cfg.timezoneMode||'').toLowerCase().includes('real')&&!String(cfg.timezoneMode||'').includes('\u771f\u5b9e')){const Native=Intl.DateTimeFormat;Intl.DateTimeFormat=function(locales,options={}){return new Native(locales,{...options,timeZone:options.timeZone||cfg.timezone})};Intl.DateTimeFormat.prototype=Native.prototype}
if(Number(cfg.geolocationModeIndex)!==1&&cfg.latitude!=null&&cfg.longitude!=null){const latitude=Number(cfg.latitude),longitude=Number(cfg.longitude);if(Number.isFinite(latitude)&&Number.isFinite(longitude)&&navigator.geolocation){const result=()=>({coords:{latitude,longitude,accuracy:30,altitude:null,altitudeAccuracy:null,heading:null,speed:null},timestamp:Date.now()});navigator.geolocation.getCurrentPosition=success=>queueMicrotask(()=>success(result()));navigator.geolocation.watchPosition=success=>{queueMicrotask(()=>success(result()));return 1};navigator.geolocation.clearWatch=()=>{}}}
if(cfg.disableWebRtc){define(globalThis,'RTCPeerConnection',undefined);define(globalThis,'webkitRTCPeerConnection',undefined)}
const patchGl=proto=>{if(!proto||!proto.getParameter)return;const native=proto.getParameter;proto.getParameter=function(p){if(p===37445&&cfg.webglVendor)return cfg.webglVendor;if(p===37446&&cfg.webglRenderer)return cfg.webglRenderer;return native.call(this,p)}};
if(String(cfg.webglMode||'').toLowerCase().includes('custom')||String(cfg.webglMode||'').includes('\u81ea\u5b9a\u4e49')||noise.webgl_noise){patchGl(globalThis.WebGLRenderingContext&&WebGLRenderingContext.prototype);patchGl(globalThis.WebGL2RenderingContext&&WebGL2RenderingContext.prototype)}
if(noise.canvas_noise&&globalThis.CanvasRenderingContext2D){const native=CanvasRenderingContext2D.prototype.getImageData;CanvasRenderingContext2D.prototype.getImageData=function(...args){const image=native.apply(this,args);for(let i=0;i<image.data.length;i+=97)image.data[i]=(image.data[i]+state%3)&255;return image}}
if(noise.client_rects_noise&&globalThis.Element){const native=Element.prototype.getBoundingClientRect;Element.prototype.getBoundingClientRect=function(){const rect=native.call(this),d=delta(17);return new DOMRect(rect.x+d,rect.y+d,rect.width,rect.height)}}
if(noise.audio_noise&&globalThis.AnalyserNode){const native=AnalyserNode.prototype.getFloatFrequencyData;AnalyserNode.prototype.getFloatFrequencyData=function(array){native.call(this,array);for(let i=0;i<array.length;i+=16)array[i]+=delta(i)}}
if(noise.font_noise&&document.fonts&&Array.isArray(cfg.customFonts)&&cfg.customFonts.length){const native=document.fonts.check.bind(document.fonts);document.fonts.check=(font,text)=>cfg.customFonts.some(name=>String(font).toLowerCase().includes(String(name).toLowerCase()))&&native(font,text)}
if(noise.media_devices_noise&&navigator.mediaDevices&&navigator.mediaDevices.enumerateDevices){navigator.mediaDevices.enumerateDevices=async()=>[{deviceId:'default',kind:'audioinput',label:'',groupId:'default',toJSON(){return this}},{deviceId:'default',kind:'audiooutput',label:'',groupId:'default',toJSON(){return this}}]}
})();)JS";

    const QString cookieImportVersion = QString::fromLatin1(
        QCryptographicHash::hash(config["cookie"].toString().toUtf8(), QCryptographicHash::Sha256).toHex());
    QByteArray background = "const RAW_COOKIES=" + jsValue(config["cookie"])
                            + ";\nconst STARTUP_URLS=" + jsValue(config["startup_url_list"])
                            + ";\nconst COOKIE_IMPORT_VERSION=" + jsValue(cookieImportVersion) + ";\n";
    background += R"JS(
function parseCookies(raw){const out=[],text=String(raw||'').trim();if(!text)return out;try{const parsed=JSON.parse(text),list=Array.isArray(parsed)?parsed:(Array.isArray(parsed.cookies)?parsed.cookies:[parsed]);for(const item of list)if(item&&item.name)out.push(item);return out}catch(_){}for(const line of text.split(/\r?\n/)){const value=line.trim();if(!value||value.startsWith('#'))continue;const cols=value.split('\t');if(cols.length>=7){out.push({domain:cols[0],path:cols[2],secure:cols[3].toUpperCase()==='TRUE',expirationDate:Number(cols[4])||undefined,name:cols[5],value:cols.slice(6).join('\t')});continue}for(const part of value.split(';')){const i=part.indexOf('=');if(i>0)out.push({name:part.slice(0,i).trim(),value:part.slice(i+1).trim()})}}return out}
async function importCookies(){for(const cookie of parseCookies(RAW_COOKIES)){const targets=cookie.domain?[`${cookie.secure?'https':'http'}://${String(cookie.domain).replace(/^\./,'')}/`]:STARTUP_URLS;for(const target of targets){try{const url=new URL(target),details={url:`${url.protocol}//${url.host}${cookie.path||'/'}`,name:String(cookie.name),value:String(cookie.value||''),path:cookie.path||'/',secure:Boolean(cookie.secure),httpOnly:Boolean(cookie.httpOnly)};if(cookie.domain)details.domain=cookie.domain;if(cookie.expirationDate)details.expirationDate=Number(cookie.expirationDate);if(cookie.sameSite)details.sameSite=String(cookie.sameSite);await chrome.cookies.set(details)}catch(error){console.warn('ProfilePower cookie import failed',error)}}}}
async function importCookiesOnce(){if(!String(RAW_COOKIES||'').trim())return;const state=await chrome.storage.local.get('cookieImportVersion');if(state.cookieImportVersion===COOKIE_IMPORT_VERSION)return;await importCookies();await chrome.storage.local.set({cookieImportVersion:COOKIE_IMPORT_VERSION})}
chrome.runtime.onInstalled.addListener(importCookiesOnce);chrome.runtime.onStartup.addListener(importCookiesOnce);
)JS";

    if (!save(QDir(path).absoluteFilePath("manifest.json"), manifest, error)
        || !save(QDir(path).absoluteFilePath("settings.js"), settings, error)
        || !save(QDir(path).absoluteFilePath("inject.js"), inject, error)
        || !save(QDir(path).absoluteFilePath("background.js"), background, error)) return false;
    if (extensionPath) *extensionPath = QDir::cleanPath(path);
    return true;
}
