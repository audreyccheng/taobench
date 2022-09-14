"use strict";(self.webpackChunkwebsite=self.webpackChunkwebsite||[]).push([[895],{3905:(e,n,t)=>{t.d(n,{Zo:()=>p,kt:()=>g});var r=t(7294);function a(e,n,t){return n in e?Object.defineProperty(e,n,{value:t,enumerable:!0,configurable:!0,writable:!0}):e[n]=t,e}function i(e,n){var t=Object.keys(e);if(Object.getOwnPropertySymbols){var r=Object.getOwnPropertySymbols(e);n&&(r=r.filter((function(n){return Object.getOwnPropertyDescriptor(e,n).enumerable}))),t.push.apply(t,r)}return t}function o(e){for(var n=1;n<arguments.length;n++){var t=null!=arguments[n]?arguments[n]:{};n%2?i(Object(t),!0).forEach((function(n){a(e,n,t[n])})):Object.getOwnPropertyDescriptors?Object.defineProperties(e,Object.getOwnPropertyDescriptors(t)):i(Object(t)).forEach((function(n){Object.defineProperty(e,n,Object.getOwnPropertyDescriptor(t,n))}))}return e}function s(e,n){if(null==e)return{};var t,r,a=function(e,n){if(null==e)return{};var t,r,a={},i=Object.keys(e);for(r=0;r<i.length;r++)t=i[r],n.indexOf(t)>=0||(a[t]=e[t]);return a}(e,n);if(Object.getOwnPropertySymbols){var i=Object.getOwnPropertySymbols(e);for(r=0;r<i.length;r++)t=i[r],n.indexOf(t)>=0||Object.prototype.propertyIsEnumerable.call(e,t)&&(a[t]=e[t])}return a}var l=r.createContext({}),c=function(e){var n=r.useContext(l),t=n;return e&&(t="function"==typeof e?e(n):o(o({},n),e)),t},p=function(e){var n=c(e.components);return r.createElement(l.Provider,{value:n},e.children)},u={inlineCode:"code",wrapper:function(e){var n=e.children;return r.createElement(r.Fragment,{},n)}},d=r.forwardRef((function(e,n){var t=e.components,a=e.mdxType,i=e.originalType,l=e.parentName,p=s(e,["components","mdxType","originalType","parentName"]),d=c(t),g=a,m=d["".concat(l,".").concat(g)]||d[g]||u[g]||i;return t?r.createElement(m,o(o({ref:n},p),{},{components:t})):r.createElement(m,o({ref:n},p))}));function g(e,n){var t=arguments,a=n&&n.mdxType;if("string"==typeof e||a){var i=t.length,o=new Array(i);o[0]=d;var s={};for(var l in n)hasOwnProperty.call(n,l)&&(s[l]=n[l]);s.originalType=e,s.mdxType="string"==typeof e?e:a,o[1]=s;for(var c=2;c<i;c++)o[c]=t[c];return r.createElement.apply(null,o)}return r.createElement.apply(null,t)}d.displayName="MDXCreateElement"},2410:(e,n,t)=>{t.r(n),t.d(n,{assets:()=>l,contentTitle:()=>o,default:()=>u,frontMatter:()=>i,metadata:()=>s,toc:()=>c});var r=t(7462),a=(t(7294),t(3905));const i={},o="Cloud Spanner",s={unversionedId:"usage/drivers/spanner",id:"usage/drivers/spanner",title:"Cloud Spanner",description:"Build Instructions",source:"@site/docs/usage/drivers/spanner.md",sourceDirName:"usage/drivers",slug:"/usage/drivers/spanner",permalink:"/docs/usage/drivers/spanner",draft:!1,tags:[],version:"current",frontMatter:{},sidebar:"usageSidebar",previous:{title:"PlanetScale / TiDB (MySQL)",permalink:"/docs/usage/drivers/mysql"},next:{title:"Yugabyte (Postgres)",permalink:"/docs/usage/drivers/yugabyte"}},l={},c=[{value:"Build Instructions",id:"build-instructions",level:2},{value:"Schema Setup",id:"schema-setup",level:2},{value:"Instance Configuration",id:"instance-configuration",level:2}],p={toc:c};function u(e){let{components:n,...t}=e;return(0,a.kt)("wrapper",(0,r.Z)({},p,t,{components:n,mdxType:"MDXLayout"}),(0,a.kt)("h1",{id:"cloud-spanner"},"Cloud Spanner"),(0,a.kt)("h2",{id:"build-instructions"},"Build Instructions"),(0,a.kt)("p",null,"Follow the CMake instructions on the Spanner quickstart guide (",(0,a.kt)("a",{parentName:"p",href:"https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/spanner/quickstart"},"https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/spanner/quickstart"),") to install the Spanner client libraries using vcpkg."),(0,a.kt)("p",null,"Then run: "),(0,a.kt)("pre",null,(0,a.kt)("code",{parentName:"pre",className:"language-shell"},"cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release .\nmake\n")),(0,a.kt)("h2",{id:"schema-setup"},"Schema Setup"),(0,a.kt)("p",null,"On your Spanner instance, create a database with the following schema:"),(0,a.kt)("pre",null,(0,a.kt)("code",{parentName:"pre",className:"language-sql"},"CREATE TABLE objects(\n    id INT64,\n    timestamp INT64,\n    value STRING(150),\n) PRIMARY KEY (id);\n\nCREATE TABLE edges(\n    id1 INT64,\n    id2 INT64,\n    type INT64,\n    timestamp INT64,\n    value STRING(150),\n) PRIMARY KEY (id1, id2, type);\n")),(0,a.kt)("h2",{id:"instance-configuration"},"Instance Configuration"),(0,a.kt)("p",null,"The Spanner ",(0,a.kt)("inlineCode",{parentName:"p"},"project-id"),", ",(0,a.kt)("inlineCode",{parentName:"p"},"instance-id"),", and ",(0,a.kt)("inlineCode",{parentName:"p"},"database")," can be configured in the ",(0,a.kt)("inlineCode",{parentName:"p"},"spanner_db/spanner.properties")," file."))}u.isMDXComponent=!0}}]);