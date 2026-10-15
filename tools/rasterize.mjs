// Rasterizes SVGs to PNGs with Playwright's Chromium: node rasterize.mjs [--scale 2] a.svg b.svg ...
// Loads each file by URL so relative references (Rack component art) resolve.
import { createRequire } from 'module';
import { execSync } from 'child_process';
import path from 'path';
const require = createRequire(import.meta.url);
const { chromium } = require(path.join(execSync('npm root -g').toString().trim(), 'playwright'));
const args = process.argv.slice(2);
let scale = 1;
if (args[0] === '--scale') { scale = Number(args[1]); args.splice(0, 2); }
const browser = await chromium.launch();
const page = await browser.newPage({ deviceScaleFactor: scale });
for (const file of args) {
	await page.goto('file://' + path.resolve(file));
	const box = await page.evaluate(() => { const r = document.documentElement.getBoundingClientRect(); return [Math.ceil(r.width), Math.ceil(r.height)]; });
	await page.setViewportSize({ width: box[0], height: box[1] });
	await page.screenshot({ path: file.replace(/\.svg$/, '.png'), omitBackground: false });
}
await browser.close();
