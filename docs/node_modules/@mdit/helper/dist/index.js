const e=e=>{let t=e.split(`
`),n=t.reduce((e,t)=>{for(let n=0;n<t.length;n++)if(t[n]!==` `&&t[n]!==`	`)return Math.min(n,e);return e},1/0);return n<1/0?t.map(e=>e.slice(n)).join(`
`):e},t=/[-/\\^$*+?.()|[\]{}]/g,n=String.raw`\$&`,r=e=>e.replaceAll(`&`,`&amp;`).replaceAll(`<`,`&lt;`).replaceAll(`>`,`&gt;`).replaceAll(`"`,`&quot;`).replaceAll(`'`,`&#39;`),i=e=>e.replaceAll(t,n),a=/\r\n?|\n/g,o=/\\([ \\!"#$%&'()*+,./:;<=>?@[\]^_`{|}~-])/gu;export{a as NEWLINE_RE,o as UNESCAPE_RE,e as dedent,r as escapeHtml,i as escapeRegExp};
//# sourceMappingURL=index.js.map