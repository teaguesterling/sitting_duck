// Every JSX tag and attribute shape the grammar admits, in one file.
// test/data/tsx/shapes.tsx is byte-identical on purpose: the javascript and tsx
// grammars both carry all 8 jsx_* node types, and JavaScriptAdapter and
// TypeScriptAdapter are SIBLINGS with separate ExtractNodeName methods. Two
// identical files are what prove the two adapters agree.
const a = <div className="x" id="y">hi</div>;
const b = <br/>;
const c = <Foo.Bar x={1}><span>{a}</span></Foo.Bar>;
const d = <svg:circle xlink:href="#z"/>;
const e = <>{a}{b}</>;
const f = <input disabled {...rest} />;
