// Jura Native Graphical Card 2.1.8 — native ESPHome entities, no MQTT or external dependencies.
(() => {
  const GRAPHIC = /*GRAPHIC*/;
  const STYLES = /*STYLES*/;
  const escapeHTML = value => String(value ?? '').replace(/[&<>"']/g, ch => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]));
  const known = value => value !== null && value !== undefined && !['','unknown','unavailable','not available'].includes(String(value).trim().toLowerCase());
  const numeric = value => known(value) && Number.isFinite(Number(value)) && Number(value) >= 0 ? Number(value) : null;
  const percentage = value => {
    if(!['number','string'].includes(typeof value)||String(value).trim()==='')return null;
    const n=Number(value);return Number.isFinite(n)&&n>=0&&n<=100?n:null;
  };
  const terminalUncertain = value => /uncertain|unconfirmed|no start confirmation|connection lost/i.test(value || '');
  const pendingCommand = value => /^(queued|reading saved|sent[; ]|awaiting start|accepted[; ]|machine active|started$)/i.test(value || '');
  const pretty = name => String(name || '').replace(/^1\s+/, '').replace(/^Hotwater Portion\(normal\)$/i,'Hot water').replace(/^Milk Portion$/i,'Milk').replace(/^2 Espressi$/,'Double espresso').replace(/^2 Ristretti$/,'Double ristretto').replace(/^2 Coffee$/,'Double coffee');
  // Exact machine labels, including Info variants, share the same display wording.
  // Keep unknown text intact and do not alter the machine's readiness decisions.
  const ALERT_MESSAGES = {
  "insert tray": "Insert the Drip Tray",
  "fill water": "Fill the Water Tank",
  "empty grounds": "Empty the Coffee Grounds Container",
  "empty tray": "Empty the Drip Tray",
  "insert coffee bin": "Insert the Coffee Grounds Container",
  "outlet missing": "Refit the Coffee Spout",
  "rear cover missing": "Refit the Rear Cover",
  "milk alert": "Milk System Alert — Check the machine display",
  "fill system": "Water System Needs Attention — Check the machine display",
  "system filling": "Filling the Water System",
  "no beans": "Fill the Bean Container",
  "welcome": "Starting Up",
  "heating up": "Heating Up",
  "coffee ready": "Ready",
  "no milk (milk sensor)": "Milk Container Empty",
  "error milk (milk sensor)": "Milk Cooler Error — Check the Milk Cooler",
  "no signal (milk sensor)": "Milk Cooler Not Responding — Check Its Connection",
  "please wait": "Please Wait",
  "coffee rinsing": "Rinsing the Coffee System",
  "ventilation closed": "Ventilation Alert — Check the machine display",
  "close powder cover": "Close the Ground Coffee Cover",
  "fill powder": "Add Ground Coffee",
  "system emptying": "Emptying the Water System",
  "not enough powder": "Not Enough Ground Coffee",
  "remove water tank": "Remove the Water Tank",
  "press rinse": "Start the Rinse Cycle",
  "goodbye": "Switching Off",
  "periphery alert": "Accessory Alert — Check the machine display",
  "powder product": "Ground Coffee Mode",
  "program-mode status": "Machine Settings Open",
  "error status": "Machine Error — Check the machine display",
  "enjoy product": "Your Drink Is Ready",
  "filter alert": "Change the Water Filter",
  "decalc alert": "Descale the Machine",
  "cleaning alert": "Clean the Machine",
  "cappu rinse alert": "Rinse the Milk System",
  "energy safe": "Energy Saving Mode",
  "active rf filter": "Water Filter Detected",
  "remotescreen": "Remote Screen Active — Check the machine display",
  "lockedkeys": "Machine Controls Locked",
  "close tab": "Machine Needs Attention — Check the machine display",
  "cappu clean alert": "Clean the Milk System",
  "info - cappu clean alert": "Clean the Milk System",
  "info - coffee clean alert": "Clean the Machine",
  "info - decalc alert": "Descale the Machine",
  "info - filter used up alert": "Change the Water Filter",
  "steam ready": "Steam Ready",
  "switchoff delay active": "Preparing to Switch Off",
  "coffee rinse": "Rinse the Coffee System",
  "coffee rinse alert": "Rinse the Coffee System",
  "filter change alert": "Change the Water Filter",
  "cappu rinse": "Rinse the Milk System",
  "cappu clean": "Clean the Milk System"
};
  const friendlyAlert = value => String(value || '').split(/([,;]\s*)/).map(part => ALERT_MESSAGES[part.trim().toLowerCase().replace(/_/g,' ')] || part).join('');
  function friendlyStage(phase, product, heatingForMilk=false) {
    const name=known(product)?pretty(product):'', milkDrink=/flat white|cappuccino|latte|milk portion/i.test(product || '');
    // Unknown TV stage codes describe no verified substage. Use the known
    // operation context for presentation; retain the original in Diagnostics.
    if(/^(?:state\s+)?0x[0-9a-f]+$/i.test(String(phase || '').trim())) {
      if(/milk.*rins/i.test(product || ''))return 'Rinsing the Milk System';
      if(/coffee.*rins/i.test(product || ''))return 'Rinsing the Coffee System';
      if(/milk.*clean/i.test(product || ''))return 'Cleaning the Milk System';
      if(/descal/i.test(product || ''))return 'Descaling the Machine';
      if(/clean/i.test(product || ''))return 'Cleaning the Machine';
      if(/filter change/i.test(product || ''))return 'Changing the Water Filter';
      return 'Machine Operation in Progress';
    }
    switch(String(phase || '').toLowerCase()) {
      case 'grinding': case 'grinding (milk drink)': return 'Grinding the Coffee';
      case 'dispensing water (milk drink)': return 'Brewing the Coffee';
      case 'dispensing coffee': return /espresso|espressi|ristrett/i.test(product || '')?'Brewing the Espresso':'Brewing the Coffee';
      case 'heating': case 'heating up': return milkDrink&&heatingForMilk?'Heating for Milk':'Heating Up';
      case 'dispensing milk': return 'Adding Milk';
      case 'dispensing milk foam': return 'Adding Milk Foam';
      case 'milk pause': return 'Letting the Milk Settle';
      case 'dispensing water': return 'Pouring Hot Water';
      case 'finished': return /rins|clean|descal|filter change/i.test(product || '')?'Cycle Complete':name?'Your '+name+' Is Ready':'Your Drink Is Ready';
      case 'rinsing': return /milk/i.test(product || '')?'Rinsing the Milk System':/coffee/i.test(product || '')?'Rinsing the Coffee System':'Rinsing';
      case 'cleaning': return /milk/i.test(product || '')?'Cleaning the Milk System':'Cleaning the Machine';
      case 'descaling': return 'Descaling the Machine';
      case 'started': return 'Preparing Your Drink';
      default: return friendlyAlert(phase);
    }
  }
  const drinkKind = name => /hotwater|hot water/i.test(name) ? 'water' : /latte/i.test(name) ? 'latte' : /cappuccino/i.test(name) ? 'cappuccino' : /flat white/i.test(name) ? 'flat' : /milk/i.test(name) ? 'milk' : /ristrett/i.test(name) ? 'ristretto' : /espresso|espressi/i.test(name) ? 'espresso' : 'coffee';
  // Cup artwork is local vector geometry, scaled by drink type; it is not a volume meter.
  function cup(name) {
    const kind=drinkKind(name),glass=['latte','milk','water'].includes(kind),double=/^2\s/.test(name);
    const shape=glass?'M26 8 H61 L57 48 Q44 53 30 48Z':kind==='espresso'||kind==='ristretto'?'M24 22 H63 L59 45 Q43 52 28 45Z':'M20 15 H66 L61 46 Q43 54 25 46Z';
    const layer=kind==='latte'?'<path d="M29 20 H59 L57 34 H30Z" class="drink-milk"/><path d="M30 34 H57 L56 45 H31Z" class="drink-coffee"/><path d="M28 13 H60 L59 20 H29Z" class="drink-foam"/>':kind==='flat'||kind==='cappuccino'?'<path d="M24 29 H62 L59 44 Q43 49 28 44Z" class="drink-coffee"/><path d="M23 21 H63 L62 29 H24Z" class="drink-foam"/>':kind==='milk'?'<path d="M29 18 H59 L56 45 H31Z" class="drink-milk"/>':kind==='water'?'<path d="M29 23 H59 L56 45 H31Z" class="drink-water"/>':`<path d="${kind==='espresso'||kind==='ristretto'?'M28 31 H59 L57 43 Q43 48 30 43Z':'M24 26 H62 L59 44 Q43 49 28 44Z'}" class="drink-coffee"/>`;
    return `<svg class="drink-art" viewBox="0 0 88 62" aria-hidden="true"><g${double?' transform="translate(-5 0)"':''}><path d="${shape}" class="cup-body"/>${layer}${glass?'':'<path d="M65 24 H69 C81 24 78 41 63 40" class="cup-handle"/>'}<path d="M18 55 H69" class="saucer"/></g>${double?'<path d="M71 13 H80 L78 26 H72Z" class="cup-handle"/><path d="M68 30 H83" class="saucer"/>':''}</svg>`;
  }
  const numberControls = [
    ['water','water_amount','water','Water','ml'],['milk','milk_amount','milk','Milk','s'],
    ['foam','milk_foam_amount','milk_foam','Milk foam','s'],['bypass','bypass_amount','bypass','Bypass water','ml'],['pause','milk_pause','milk_pause','Milk pause','s']
  ];
  const choiceControls=[['strength','coffee_strength','current_strength_choices','Strength'],['temperature','temperature','current_temperature_choices','Temperature'],['grinder','grinder_ratio','current_grinder_choices','Grinder ratio']];
  const maintenance=[['cleaning','Cleaning'],['filterchange','Filter'],['decalc','Descaling'],['cappurinse','Milk rinses'],['coffeerinse','Coffee rinses'],['cappuclean','Milk cleaning']];
  const recipeKeys={strength:'strength',water_ml:'water',milk_seconds:'milk',foam_seconds:'foam',temperature:'temperature',bypass_ml:'bypass',pause_seconds:'pause'};
  class JuraNativeCard extends HTMLElement {
    constructor(){ super();this.attachShadow({mode:'open'});this._timers=new Set();this._edits=new Map();this._selection=null;this._brewLock=false;this._writeError=''; }
    setConfig(config){
      const defaults={my_recipes:'expanded',choose_a_drink:'expanded',more_drinks:'collapsed',brew:'expanded',maintenance:'collapsed',diagnostics:'collapsed'};
      if(config.sections!==undefined&&(!config.sections||typeof config.sections!=='object'||Array.isArray(config.sections)))throw new Error('sections must be a mapping');
      for(const [key,value]of Object.entries(config.sections||{}))if(!(key in defaults)||!['expanded','collapsed','hidden'].includes(value))throw new Error('Invalid section setting: '+key+' (use expanded, collapsed or hidden)');
      this._sections={...defaults,...config.sections};

      if(config.hold_to_brew!==undefined&&typeof config.hold_to_brew!=='boolean')throw new Error('hold_to_brew must be true or false');
      this._cancelHold?.();this._holdBrewPending=false;this._holdJob=null;this._heldClicks=new WeakSet();
      if(config.recipes!==undefined&&!Array.isArray(config.recipes))throw new Error('recipes must be a list');
      this._recipes=(config.recipes||[]).map((recipe,index)=>{
        if(!recipe||typeof recipe.name!=='string'||!recipe.name.trim()||typeof recipe.base_drink!=='string'||!recipe.base_drink.trim())throw new Error(`Recipe ${index+1} needs a name and base_drink`);
        const overrides=recipe.overrides??{};
        if(typeof overrides!=='object'||Array.isArray(overrides))throw new Error(`Recipe ${recipe.name}: overrides must be a mapping`);
        for(const [key,value]of Object.entries(overrides)){
          if(!Object.hasOwn(recipeKeys,key))throw new Error(`Recipe ${recipe.name}: unsupported override ${key}`);
          if(['strength','temperature'].includes(key)){if(!['string','number'].includes(typeof value)||String(value).trim()==='')throw new Error(`Recipe ${recipe.name}: invalid ${key}`);}
          else if(typeof value!=='number'||!Number.isInteger(value)||value<0)throw new Error(`Recipe ${recipe.name}: ${key} must be a nonnegative integer`);
        }
        return {name:recipe.name.trim(),base_drink:recipe.base_drink.trim(),overrides:{...overrides}};
      });
      this._recipeRun=null;this._customRecipe=null;this._recipeError='';
      this._config={entity_prefix:'jura_e8',name:'Jura E8',...config};
      const p=this._config.entity_prefix,sensor=suffix=>`sensor.${p}_${suffix}`,binary=suffix=>`binary_sensor.${p}_${suffix}`;
      const textKeys=['status','status_detail','active_alerts','current_product','progress_state','stage_unit','command_result','requested_product','recipe_source','recipe_status','saved_recipe_raw','command_recipe_source','command_recipe_status','command_recipe_raw','session','profile','tf_raw','tv_raw','progress','stage_progress','stage_current','stage_target','product_code','stage_code','total_products','uart_timeouts','uart_framing_errors'];
      this._entities=Object.fromEntries(textKeys.map(k=>[k,sensor(`jura_${k}`)]));
      for(const k of ['connected','brew_active','alerts_stale','recipe_loading','recipe_modified'])this._entities[k]=binary(`jura_${k}`);
      for(const k of ['tray_status','water_tank_status','grounds_level','changed_counters'])this._entities[k]=sensor(k);
      for(const k of ['empty_grounds','insert_coffee_bin','no_beans','fill_water','insert_tray','empty_tray'])this._entities[k]=binary(k);
      this._entities.drink=`select.${p}_jura_drink`;
      for(const [key,field,choices] of choiceControls){this._entities[key]=`select.${p}_jura_${field}`;this._entities[choices]=sensor(`jura_${choices}`);}
      for(const [key,field,bounds] of numberControls){this._entities[key]=`number.${p}_jura_${field}`;for(const limit of ['min','max','step'])this._entities[`${key}_${limit}`]=sensor(`jura_${bounds}_${limit}`);}
      for(const key of ['brew','reset_recipe','refresh'])this._entities[key]=`button.${p}_jura_${key}`;
      this._entities.detected_model=sensor('detected_jura_model');
      for(const [key]of maintenance){this._entities[`${key}_count`]=sensor(`${key}_count`);this._entities[`${key}_maintenance`]=sensor(`${key}_maintenance`);}
      this._cooler=config.coolcontrol!==false;
      if(this._cooler){this._entities.milk_level=config.coolcontrol?.level||'sensor.jura_coolcontrol_jura_coolcontrol_level';this._entities.milk_temperature=config.coolcontrol?.temperature||'sensor.jura_coolcontrol_jura_coolcontrol_temperature';}
      this._entities.coolcontrol_override=`switch.${p}_jura_cool_control_override`;
      Object.assign(this._entities,config.entities||{});
      this._signature=null;this._tileSignature=null;this._build();this._update();
    }
    set hass(hass){this._hass=hass;this._update();}
    getCardSize(){return 15;}
    getGridOptions(){return {columns:12,min_columns:6};}
    static getStubConfig(){return {name:'Jura E8',entity_prefix:'jura_e8'};}
    disconnectedCallback(){this._cancelHold();this._holdJob=null;this._holdBrewPending=false;for(const id of this._timers)clearTimeout(id);this._timers.clear();}
    connectedCallback(){this._update();}
    _later(fn,delay){const id=setTimeout(()=>{this._timers.delete(id);fn();},delay);this._timers.add(id);return id;}
    _entity(key){return this._hass?.states?.[this._entities[key]];}
    _state(key){return this._entity(key)?.state;}
    _stamp(key){const e=this._entity(key);return e?`${e.state}|${e.last_updated||e.last_changed||''}`:'';}
    _format(value,digits=0){const n=numeric(value);return n===null?'—':n.toLocaleString(this._hass?.locale?.language||undefined,{maximumFractionDigits:digits,minimumFractionDigits:digits});}
    _text(key,fallback='—'){return known(this._state(key))?this._state(key):fallback;}
    _info(key){const entityId=this._entities[key];if(this._hass?.states?.[entityId])this.dispatchEvent(new CustomEvent('hass-more-info',{detail:{entityId},bubbles:true,composed:true}));}
    _build(){
      this.shadowRoot.innerHTML=`<style>${STYLES}</style><ha-card class="jura-card">
       <header><div><strong>${escapeHTML(this._config.name)}</strong><span class="subtitle">Coffee station</span></div><span class="status"><span class="dot"></span><span id="status">Connecting</span></span></header>
       <div id="detail" class="status-detail" role="status"></div><div id="alerts" class="alert-banner" hidden></div>
       <div class="machine-stage">${GRAPHIC}</div>
       <section id="operation" class="operation" hidden aria-label="Observed machine operation"><div class="operation-heading"><strong id="actual-product"></strong><span id="overall"></span></div><div class="phase-row"><span id="phase"></span><span id="stage"></span></div><div id="progress-track" class="progress-track" role="progressbar" aria-label="Overall drink progress" aria-valuemin="0" aria-valuemax="100"><div id="progress-fill"></div></div></section>
       <div class="readings">${this._reading('water_tank_status','water-check','Water')}${this._reading('tray_status','tray','Tray')}${this._reading('empty_grounds','hockey-puck','Grounds')}${this._reading('no_beans','seed-outline','Beans')}${this._cooler?this._reading('milk_level','water-percent','Milk'):''}</div>
       <section id="cooler-override" class="cooler-override" hidden><div><strong id="cooler-override-label">Ignore milk cooler fault</strong><p id="cooler-override-help"></p></div><button id="cooler-override-toggle" type="button" role="switch" aria-label="Ignore milk cooler fault" aria-checked="false"><span></span></button></section>
       <details id="my-recipes" class="drink-section section-disclosure" hidden aria-label="My Recipes"><summary class="section-label">MY RECIPES<span>${this._config.hold_to_brew?'Tap or hold to brew':'Select, then brew'}</span></summary><div id="recipe-tiles" class="drink-tiles">${this._recipes.map((recipe,index)=>`<button class="drink-tile" data-recipe="${index}" aria-label="Select ${escapeHTML(recipe.name)}" aria-pressed="false">${cup(recipe.base_drink)}<span>${escapeHTML(recipe.name)}</span><small>Based on ${escapeHTML(pretty(recipe.base_drink))}</small></button>`).join('')}</div></details>
       <details id="choose-a-drink" class="drink-section section-disclosure" aria-label="Drink selection"><summary class="section-label">CHOOSE A DRINK<span>${this._config.hold_to_brew?'Tap or hold to brew':'Select, then brew'}</span></summary><div id="drink-tiles" class="drink-tiles"></div></details>
       <details id="more-drinks" class="more-drinks-section" aria-label="More drinks"><summary>More drinks<span id="more-count"></span></summary><div id="extra-tiles" class="drink-tiles"></div></details>
       <details id="recipe" class="recipe section-disclosure" aria-label="Selected drink recipe"><summary class="section-label">BREW SETTINGS</summary><div class="recipe-heading"><strong id="selected-name">Select a drink</strong><span id="recipe-source" class="source-chip"></span></div><div id="recipe-summary"></div><div id="recipe-message" class="recipe-message"></div><details id="adjust"><summary>Adjust this drink<span id="modified"></span></summary><div class="recipe-controls">${choiceControls.map(([key,,,label])=>`<label data-control="${key}" hidden>${label}<select data-input="${key}" aria-label="${label}"></select></label>`).join('')}${numberControls.map(([key,,,label,unit])=>`<label data-control="${key}" hidden>${label} <span class="unit">${unit}</span><input type="number" data-input="${key}" aria-label="${label}" inputmode="numeric"><span class="bounds" data-bounds="${key}"></span></label>`).join('')}</div><button id="reset" class="text-button" type="button">Reset to saved recipe</button></details><button id="brew" class="brew-button" type="button" disabled><ha-icon icon="mdi:coffee-maker"></ha-icon><span>Brew</span></button><div id="brew-reason" class="brew-reason"></div><div id="command" class="command" role="status" hidden></div><button id="acknowledge" class="text-button" type="button" hidden>I checked the machine</button><div id="error" class="error" role="alert" hidden></div></details>
       <details id="maintenance"><summary><span><ha-icon icon="mdi:wrench-outline"></ha-icon>Maintenance & totals</span><span id="total"></span></summary><div class="maintenance-meters">${maintenance.slice(0,3).map(([key,label])=>`<button class="maintenance-meter" data-info="${key}_maintenance"><span>${label}</span><strong data-number="${key}_maintenance" data-unit="%">—</strong><div class="meter-track"><div data-meter="${key}_maintenance"></div></div></button>`).join('')}</div><div class="detail-rows">${maintenance.map(([key,label])=>`<button data-info="${key}_count" class="detail-row"><span>${label} count</span><strong data-number="${key}_count">—</strong></button>`).join('')}</div><div id="all-counters" class="detail-rows"></div><button id="refresh" class="text-button">Refresh readings</button></details>
       <details id="diagnostics"><summary><span><ha-icon icon="mdi:pulse"></ha-icon>Diagnostics</span></summary>${['detected_model','profile','session','current_product','progress_state','stage_code','stage_progress','requested_product','command_result','command_recipe_source','command_recipe_status','recipe_status','saved_recipe_raw','command_recipe_raw','tf_raw','tv_raw','uart_timeouts','uart_framing_errors','grounds_level','changed_counters'].map(key=>`<button class="diagnostic-row" data-info="${key}"><span>${key.replaceAll('_',' ')}</span><code data-diagnostic="${key}">—</code></button>`).join('')}<div class="diagnostic-note">Legacy grounds and raw counters are unverified. They are not used for drink totals or readiness.</div></details>
      </ha-card>`;
      this._root=this.shadowRoot;this._card=this._root.querySelector('ha-card');
      this._root.addEventListener('click',event=>{const node=event.target.closest('[data-info]');if(node)this._info(node.dataset.info);});
      this._root.querySelectorAll('[data-recipe]').forEach(button=>this._bindDrinkButton(button,'recipe',Number(button.dataset.recipe)));
      for(const [key,id]of Object.entries({my_recipes:'my-recipes',choose_a_drink:'choose-a-drink',more_drinks:'more-drinks',brew:'recipe',maintenance:'maintenance',diagnostics:'diagnostics'})){const section=this._root.querySelector('#'+id);section.hidden=this._sections[key]==='hidden';section.open=this._sections[key]==='expanded';}
      this._root.querySelector('#cooler-override-toggle').addEventListener('click',()=>this._toggleCoolerOverride());
      this._root.querySelector('#brew').addEventListener('click',()=>this._brew());
      this._root.querySelector('#reset').addEventListener('click',()=>this._reset());
      this._root.querySelector('#refresh').addEventListener('click',()=>this._service('button','press','refresh'));
      this._root.querySelector('#acknowledge').addEventListener('click',()=>{if(this._state('status')!=='ready'||this._state('connected')!=='on')return;this._ack=this._stamp('command_result');this._brewLock=false;this._localUncertain=false;this._update(true);});
      this._root.querySelectorAll('[data-input]').forEach(input=>input.addEventListener('change',()=>this._edit(input.dataset.input,input.value)));
      if(!this._cooler)this._root.querySelectorAll('.cooler,.milk-part').forEach(el=>el.setAttribute('display','none'));
    }
    _reading(key,icon,label){return `<button class="reading" data-info="${key}"><ha-icon icon="mdi:${icon}"></ha-icon><strong data-reading="${key}">—</strong><span>${label}</span></button>`;}
    async _service(domain,service,key,data={}){
      try{await this._hass.callService(domain,service,{entity_id:this._entities[key],...data});return true;}
      catch(error){this._writeError=error?.message||String(error);this._update(true);return false;}
    }
    _options(){return (this._entity('drink')?.attributes?.options||[]).filter(known);}
    _countEntity(name){const wanted=`${name} Count`.toLowerCase(),prefix=`sensor.${this._config.entity_prefix}_`;return Object.keys(this._hass?.states||{}).find(id=>id.startsWith(prefix)&&String(this._hass.states[id].attributes?.friendly_name||'').toLowerCase().replace(new RegExp(`^${this._config.name.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')}\\s+`,'i'),'')===wanted)||`${prefix}${name.toLowerCase().replace(/[^a-z0-9]+/g,'_').replace(/^_|_$/g,'')}_count`;}
    _renderTiles(){
      const options=this._options(),signature=JSON.stringify(options);if(signature===this._tileSignature)return;
      this._tileSignature=signature;const primary=['Espresso','Coffee','1 Flat White','Flat White','Cappuccino','Latte Macchiato','Ristretto'];
      const main=primary.map(label=>options.find(x=>x.toLowerCase()===label.toLowerCase())).filter((x,i,a)=>x&&a.indexOf(x)===i),extra=options.filter(x=>!main.includes(x));
      const tiles=items=>items.map(name=>`<button class="drink-tile" data-drink="${escapeHTML(name)}" aria-label="Select ${escapeHTML(pretty(name))}" aria-pressed="false">${cup(name)}<span>${escapeHTML(pretty(name))}</span><small data-count="${escapeHTML(name)}">— made</small></button>`).join('');
      this._root.querySelector('#drink-tiles').innerHTML=tiles(main);this._root.querySelector('#extra-tiles').innerHTML=tiles(extra);
      this._root.querySelector('#more-drinks').hidden=!extra.length||this._sections.more_drinks==='hidden';this._root.querySelector('#more-count').textContent=extra.length?String(extra.length):'';
      this._root.querySelectorAll('[data-drink]').forEach(button=>this._bindDrinkButton(button,'drink',button.dataset.drink));
      this._root.querySelector('#all-counters').innerHTML=options.map(name=>`<button class="detail-row" data-counter-info="${escapeHTML(name)}"><span>${escapeHTML(pretty(name))}</span><strong data-count="${escapeHTML(name)}">—</strong></button>`).join('');
      this._root.querySelectorAll('[data-counter-info]').forEach(button=>button.addEventListener('click',()=>{const id=this._countEntity(button.dataset.counterInfo);if(this._hass.states[id])this.dispatchEvent(new CustomEvent('hass-more-info',{detail:{entityId:id},bubbles:true,composed:true}));}));
    }
    _bindDrinkButton(button,kind,value){
      const select=()=>kind==='recipe'?this._selectRecipe(value):this._selectDrink(value);
      button.addEventListener('click',event=>{
        if(this._heldClicks.has(button)){this._heldClicks.delete(button);event.preventDefault();return;}
        if(!this._holdBrewPending)select();
      });
      if(!this._config.hold_to_brew)return;
      button.title='Tap to select; hold for 1.5 seconds to prepare and brew';
      button.classList.add('hold-enabled');
      button.addEventListener('contextmenu',event=>event.preventDefault());
      button.addEventListener('pointerdown',event=>{
        this._heldClicks.delete(button);
        if(!event.isPrimary||event.button!==0||button.disabled||this._holdBrewPending||this._brewReason())return;
        this._cancelHold();
        const hold={button,kind,value,pointerId:event.pointerId};this._hold=hold;
        button.classList.add('holding');
        hold.cancel=()=>this._cancelHold();window.addEventListener('scroll',hold.cancel,true);window.addEventListener('blur',hold.cancel);
        hold.timer=setTimeout(()=>{
          if(this._hold!==hold)return;
          this._heldClicks.add(button);this._cancelHold();
          this._holdAndBrew(kind,value);
        },1500);
      });
      for(const type of ['pointerup','pointercancel','pointerleave','lostpointercapture'])button.addEventListener(type,()=>{if(this._hold?.button===button)this._cancelHold();});
    }
    _cancelHold(){
      const hold=this._hold;if(!hold)return;
      clearTimeout(hold.timer);hold.button.classList.remove('holding');
      window.removeEventListener('scroll',hold.cancel,true);window.removeEventListener('blur',hold.cancel);this._hold=null;
    }
    async _holdAndBrew(kind,value){
      if(this._holdBrewPending||this._brewReason())return;
      if(this._milkReason(kind==='recipe'?this._recipes[value]?.base_drink:value))return;
      const job={};this._holdJob=job;this._holdBrewPending=true;this._writeError='';this._update(true);
      const valid=()=>this._holdJob===job&&this.isConnected&&this._state('connected')==='on'&&this._state('status')==='ready'&&this._state('alerts_stale')==='off'&&!pendingCommand(this._state('command_result'));
      try{
        if(kind==='recipe')await this._selectRecipe(value);
        else if(this._state('drink')===value)await this._reset();
        else await this._selectDrink(value);
        const started=Date.now();
        while(this._selection||this._recipeRun||this._state('recipe_loading')!=='off'||this._edits.size){
          if(!valid()||this._editUnconfirmed||this._recipeError||this._writeError||Date.now()-started>18000)throw new Error('Hold-to-brew cancelled: recipe or machine state was not confirmed.');
          await new Promise(resolve=>setTimeout(resolve,100));
        }
        const matches=kind==='recipe'?this._customRecipe?.index===value&&this._state('drink')===this._recipes[value]?.base_drink:!this._customRecipe&&this._state('drink')===value;
        if(!valid()||!matches||this._brewReason()||this._writeError||this._state('recipe_source')!=='Machine')throw new Error('Hold-to-brew cancelled. Check the recipe and machine before brewing.');
        await this._brew(true);
      }catch(error){if(this._holdJob===job){this._writeError=error.message;}}
      finally{if(this._holdJob===job){this._holdJob=null;this._holdBrewPending=false;this._update(true);}}
    }
    async _selectDrink(name,fromRecipe=false){
      if(this._milkReason(name)||!this._options().includes(name)||this._selection||this._brewLock||this._state('connected')!=='on'||this._state('status')!=='ready'||pendingCommand(this._state('command_result')))return;
      if(this._recipeRun&&!fromRecipe)return;
      if(!fromRecipe){this._customRecipe=null;this._recipeError='';}
      if(name===this._state('drink')){if(this._state('recipe_modified')==='on'||this._editUnconfirmed)await this._reset();else this._update(true);return;}
      this._writeError='';this._selection={name,sawLoading:false,recipeStamp:this._stamp('recipe_status')};this._update(true);
      if(!await this._service('select','select_option','drink',{option:name})){this._selection=null;this._update(true);return;}
      this._selectionTimeout();
    }
    _selectionTimeout(){const selection=this._selection;if(!selection)return;this._later(()=>{if(this._selection===selection){this._selection=null;this._editUnconfirmed=true;this._writeError='Recipe change unconfirmed. Reset the recipe before brewing.';this._update(true);}},15000);}
    async _reset(fromRecipe=false){
      if(this._recipeRun&&!fromRecipe)return;
      if(!fromRecipe){this._customRecipe=null;this._recipeError='';}
      if(this._state('connected')!=='on'||this._state('status')!=='ready'||pendingCommand(this._state('command_result'))||this._brewLock)return;
      this._writeError='';this._editUnconfirmed=false;this._edits.clear();this._selection={name:this._state('drink'),sawLoading:false,recipeStamp:this._stamp('recipe_status'),requireFresh:true};this._update(true);
      if(!await this._service('button','press','reset_recipe')){this._selection=null;this._editUnconfirmed=true;this._update(true);return;}this._selectionTimeout();
    }
    async _waitRecipe(test,run,timeout=16000){
      const started=Date.now();
      while(!test()){
        if(this._recipeRun!==run||!this.isConnected||this._state('connected')!=='on'||this._state('status')!=='ready'||pendingCommand(this._state('command_result')))throw new Error('Recipe preparation interrupted. Select the recipe again when the machine is ready.');
        if(this._editUnconfirmed||Date.now()-started>timeout)throw new Error('Recipe settings were not confirmed. Reset or select the recipe again before brewing.');
        await new Promise(resolve=>setTimeout(resolve,100));
      }
      if(this._recipeRun!==run||this._state('connected')!=='on'||this._state('status')!=='ready')throw new Error('Recipe preparation interrupted.');
    }
    async _selectRecipe(index){
      if(this._recipeRun||this._selection||this._edits.size||this._brewLock||this._state('connected')!=='on'||this._state('status')!=='ready'||pendingCommand(this._state('command_result')))return;
      const recipe=this._recipes[index];if(!recipe)return;
      this._customRecipe={...recipe,index};this._recipeError='';this._writeError='';
      if(this._milkReason(recipe.base_drink))return;
      if(!this._options().includes(recipe.base_drink)){this._recipeError=`Base drink "${recipe.base_drink}" is unavailable. Use the component's exact drink label.`;this._update(true);return;}
      const run={};this._recipeRun=run;this._editUnconfirmed=false;this._update(true);
      try{
        // Always reload the saved baseline, including when the same drink is selected.
        if(this._state('drink')===recipe.base_drink)await this._reset(true);else await this._selectDrink(recipe.base_drink,true);
        if(this._writeError)throw new Error(this._writeError);
        await this._waitRecipe(()=>!this._selection&&this._state('recipe_loading')==='off'&&this._state('drink')===recipe.base_drink,run);
        if(this._state('recipe_source')!=='Machine')throw new Error('Saved machine recipe unavailable. Custom recipes require a confirmed machine-saved baseline.');
        const edits=Object.entries(recipe.overrides).map(([field,value])=>({field,key:recipeKeys[field],value:String(value)}));
        // Validate the complete recipe before sending any override.
        for(const edit of edits){
          const {field,key,value}=edit,choice=['strength','temperature'].includes(key),bounds=this._bounds(key),n=Number(value);
          if(!this._entity(key)||!known(this._state(key))||(choice?!this._choices(key).includes(value):!bounds||n<bounds.min||n>bounds.max||Math.abs((n-bounds.min)/bounds.step-Math.round((n-bounds.min)/bounds.step))>1e-7))throw new Error(`${recipe.name}: ${field} (${value}) is not supported for ${pretty(recipe.base_drink)}. Check this drink's choices, range and step.`);
        }
        for(const {key,value}of edits){
          if(String(this._state(key))===value)continue;
          await this._edit(key,value);
          if(this._editUnconfirmed||this._writeError)throw new Error(this._writeError||'Could not apply the recipe.');
          await this._waitRecipe(()=>!this._edits.has(key),run,11000);
        }
      }catch(error){if(this._recipeRun===run){this._recipeError=error.message;this._writeError='';}}
      finally{if(this._recipeRun===run){this._recipeRun=null;this._update(true);}}
    }
    _bounds(key){const min=numeric(this._state(`${key}_min`)),max=numeric(this._state(`${key}_max`)),step=numeric(this._state(`${key}_step`));return min!==null&&max!==null&&step!==null&&step>0&&max>=min?{min,max,step}:null;}
    _choices(key){const spec=choiceControls.find(x=>x[0]===key);return spec?String(this._state(spec[2])||'').split('|').map(s=>s.trim()).filter(known):[];}
    async _edit(key,value){
      if(this._selection||this._state('recipe_loading')!=='off'||this._state('status')!=='ready'||this._state('connected')!=='on'||pendingCommand(this._state('command_result'))||this._brewLock)return;
      const choice=choiceControls.some(x=>x[0]===key);const bounds=this._bounds(key);const number=Number(value);
      if(choice?!this._choices(key).includes(value):(!bounds||value.trim()===''||!Number.isInteger(number)||number<bounds.min||number>bounds.max||Math.abs((number-bounds.min)/bounds.step-Math.round((number-bounds.min)/bounds.step))>1e-7)){
        const input=this._root.querySelector(`[data-input="${key}"]`);if(input)input.value=this._state(key);
        this._writeError='Choose a supported value within this drink’s range and step.';this._update(true);return;
      }
      const expected=choice?value:String(number);this._edits.set(key,expected);this._writeError='';this._update(true);
      const ok=await this._service(choice?'select':'number',choice?'select_option':'set_value',key,choice?{option:value}:{value:number});
      if(!ok){this._edits.delete(key);this._editUnconfirmed=true;this._update(true);return;}
      this._later(()=>{if(this._edits.get(key)===expected){this._edits.delete(key);this._editUnconfirmed=true;this._writeError='Setting change unconfirmed. Reset the recipe before brewing.';this._update(true);}},10000);
    }
    async _toggleCoolerOverride(){
      const state=this._state('coolcontrol_override');
      if(this._overridePending||!['on','off'].includes(state)||this._state('connected')!=='on'||this._state('brew_active')==='on')return;
      const target=state==='on'?'off':'on';this._overridePending=target;this._writeError='';this._update(true);
      if(!await this._service('switch',target==='on'?'turn_on':'turn_off','coolcontrol_override')){this._overridePending=null;this._update(true);return;}
      this._later(()=>{if(this._overridePending===target){this._overridePending=null;this._writeError='Milk cooler override change not confirmed. Check the switch state.';this._update(true);}},10000);
    }
    _milkReason(name,includeOverridden=false){
      if(!includeOverridden&&this._state('coolcontrol_override')==='on')return '';

      // Use the Jura's own alerts, rather than inferring readiness from cooler telemetry.
      if(!/flat white|cappuccino|latte|milk|foam/i.test(name||''))return '';
      const labels=this._text('active_alerts','').split(/[,;]/).map(s=>s.trim().toLowerCase().replace(/_/g,' '));
      for(const label of ['no milk (milk sensor)','error milk (milk sensor)','no signal (milk sensor)'])
        if(labels.includes(label))return friendlyAlert(label);
      return '';
    }
    _brewReason(){
      if(this._state('connected')!=='on')return 'Machine disconnected';
      if(this._state('brew_active')==='on')return 'Machine operation in progress';
      if(this._state('status')!=='ready')return friendlyAlert(this._text('status_detail','Waiting for the machine to be ready'));
      if(this._state('alerts_stale')!=='off')return 'Waiting for fresh machine alerts';
      if(this._milkReason(this._state('drink')))return this._milkReason(this._state('drink'));
      if(this._recipeRun)return 'Preparing your custom recipe…';
      if(this._recipeError)return 'Custom recipe not ready — reset or choose a recipe';
      if(this._selection||this._state('recipe_loading')!=='off')return 'Loading the saved recipe…';
      if(this._edits.size)return 'Applying your settings…';
      if(this._editUnconfirmed)return 'Reset the recipe to verify its settings';
      if(this._brewLock||pendingCommand(this._state('command_result')))return this._localUncertain?'Check the machine before another request':'Request in progress…';
      if(this._localUncertain||(terminalUncertain(this._state('command_result'))&&this._ack!==this._stamp('command_result')))return 'Check the previous request before brewing again';
      if(!this._options().includes(this._state('drink')))return 'Select an available drink';
      if(!['Machine','Profile defaults'].includes(this._state('recipe_source')))return 'Recipe source unavailable';
      if(!this._entity('brew')||!known(this._state('brew'))&&this._state('brew')!=='unknown')return 'Brew control unavailable';
      return '';
    }
    async _brew(fromHold=false){
      if(this._holdBrewPending&&!fromHold)return;
      if(this._brewReason())return;
      this._brewLock=true;this._localUncertain=false;this._brewStamp=this._stamp('command_result');this._writeError='';this._update(true);
      if(!await this._service('button','press','brew')){this._localUncertain=true;this._brewLock=false;this._update(true);return;}
      this._later(()=>{if(this._brewLock){this._localUncertain=true;this._brewLock=false;this._writeError='No command confirmation received. Check the machine; this request will not be retried.';this._update(true);}},25000);
    }
    _update(force=false){
      if(!this._hass||!this._root)return;
      const signature=JSON.stringify([Object.keys(this._hass.states).filter(id=>id.includes(this._config.entity_prefix)||id===this._entities.milk_level||id===this._entities.milk_temperature||Object.values(this._entities).includes(id)).map(id=>[id,this._hass.states[id].state,this._hass.states[id].last_updated,this._hass.states[id].attributes?.options]),this._hass.locale?.language]);
      if(!force&&signature===this._signature)return;this._signature=signature;
      const $=selector=>this._root.querySelector(selector),set=(selector,text)=>{$(selector).textContent=text;};
      const status=this._text('status','unknown').toLowerCase(),connected=this._state('connected')==='on',stale=this._state('alerts_stale')!=='off',phase=this._text('progress_state',''),code=numeric(this._state('stage_code'));
      const activeAlerts=this._text('active_alerts',''),alertLabels=activeAlerts.split(/[,;]/).map(s=>s.trim().toLowerCase());
      // Fresh TF shutdown status can outlive the last rinse TV frame. Preserve a
      // rinse explicitly still flagged by TF, and never infer standby or ready.
      const shuttingDown=connected&&!stale&&alertLabels.includes('switchoff delay active')&&!alertLabels.includes('coffee rinsing')&&(!/rins/i.test(phase)||/coffee rins/i.test(this._text('current_product','')));
      const active=connected&&!shuttingDown&&(status==='busy'||this._state('brew_active')==='on'),finished=connected&&!shuttingDown&&phase==='Finished';
      if(this._brewLock&&this._stamp('command_result')!==this._brewStamp){this._brewLock=false;this._localUncertain=false;}
      if(this._selection){
        if(this._state('recipe_loading')==='on')this._selection.sawLoading=true;
        if(this._state('drink')===this._selection.name&&this._state('recipe_loading')==='off'&&(this._selection.sawLoading||!this._selection.requireFresh&&this._text('recipe_status','')===`Saved recipe loaded for ${this._selection.name}`||this._stamp('recipe_status')!==this._selection.recipeStamp))this._selection=null;
      }
      for(const [key,expected]of this._edits){if(String(this._state(key))===expected||numeric(this._state(key))!==null&&String(Number(this._state(key)))===expected)this._edits.delete(key);}
      const observedProduct=this._text('current_product','');
      if(!active||this._phaseProduct!==observedProduct||/grinding/i.test(phase))this._coffeeBeforeMilk=false;
      if(active&&phase==='Dispensing water (milk drink)')this._coffeeBeforeMilk=true;
      this._phaseProduct=observedProduct;
      const displayPhase=friendlyStage(phase,observedProduct,this._coffeeBeforeMilk);
      const mode=!connected?(status==='connecting'?'connecting':'offline'):shuttingDown?'switching_off':status==='blocked'?'blocked':status==='heating'?'heating':active?'busy':status;
      this._card.dataset.mode=mode;this._card.dataset.phase=active?(code===33?'heating':code===49||code===57?'grinding':code===50?'milk':code===52?'foam':code===51?'pause':code===55||code===60?'coffee':code===65?'water':/rins|clean|descal/i.test(displayPhase)?'rinse':'working'):mode==='heating'?'heating':finished?'finished':'idle';
      const statusNames={switching_off:'Switching off',ready:'Ready',busy:'Making a drink',blocked:'Needs attention',heating:'Heating',finishing:'Finishing',not_ready:'Not ready',connecting:'Connecting',offline:'Offline',unknown:'Unknown'};
      set('#status',statusNames[mode]||'Unknown');set('#detail',shuttingDown?'Preparing to Switch Off':!connected&&status!=='connecting'?'Machine disconnected':active?displayPhase||'Machine operation in progress':friendlyAlert(this._text('status_detail','Waiting for machine status')));
      // Active operations report TV progress instead of TF. Suppress only the
      // expected stale-status notice, preserving physical machine alert labels.
      const suppressStaleNotice=active||connected&&status==='finishing';
      const visibleAlerts=suppressStaleNotice?activeAlerts.split(/[,;]/).filter(label=>!/^(?:unknown\s*\(\s*stale alerts\s*\)|alert readings are stale|stale alerts|alerts stale)$/i.test(label.trim().replace(/_/g,' '))).join(', '):activeAlerts;
      const alerts=[];if(visibleAlerts&&visibleAlerts.toLowerCase()!=='none')alerts.push(friendlyAlert(visibleAlerts));if(stale&&!suppressStaleNotice&&!/stale/i.test(activeAlerts))alerts.push('Alert readings are stale');
      $('#alerts').hidden=!connected||!alerts.length;set('#alerts',connected?alerts.join(' · '):'');
      const freshReady=connected&&!stale&&status==='ready';
      const operation=connected&&!shuttingDown&&!freshReady&&(active||finished||/progress timeout/i.test(phase));$('#operation').hidden=!operation;
      set('#actual-product',pretty(this._text('current_product','Machine operation')));set('#phase',displayPhase||'Waiting for progress');
      const overall=connected?percentage(this._state('progress')):null;
      const stageProgress=active&&!/progress timeout|^idle$|^finished$/i.test(phase)?percentage(this._state('stage_progress')):null;
      const useStage=overall===null&&stageProgress!==null,percent=overall??stageProgress;
      const percentText=percent===null?'':useStage?`${displayPhase||'Current stage'} — ${this._format(percent)}%`:`${this._format(percent)}%`;
      $('#overall').hidden=percent===null;set('#overall',percentText);
      $('#progress-track').hidden=percent===null;$('#progress-track').setAttribute('aria-valuenow',percent===null?'0':String(percent));
      $('#progress-track').setAttribute('aria-label',useStage?`${displayPhase||'Current stage'} stage progress`:'Overall drink progress');
      $('#progress-track').setAttribute('aria-valuetext',percentText);$('#progress-fill').style.width=`${percent??0}%`;
      const current=numeric(this._state('stage_current')),target=numeric(this._state('stage_target')),unit=this._text('stage_unit','');
      set('#stage',(current!==null||target!==null)&&['ml','s','level'].includes(unit)?`${this._format(current)} / ${this._format(target)} ${unit}`:'');
      const screen=shuttingDown?'Switching Off':connected?(active?(/grind/i.test(phase)?'Grinding':/brewing/i.test(displayPhase)?'Brewing':phase==='Dispensing milk foam'?'Adding Foam':phase==='Dispensing milk'?'Adding Milk':/heating/i.test(phase)?'Heating':phase==='Milk pause'?'Settling Milk':phase==='Dispensing water'?'Hot Water':/rinsing the milk/i.test(displayPhase)?'Milk Rinse':/rinsing the coffee/i.test(displayPhase)?'Coffee Rinse':displayPhase.slice(0,16)):statusNames[status]||'Unknown'):'Offline';
      set('#jura-screen-status',screen);
      const reading=(key,value,alert=false)=>{const el=$(`[data-reading="${key}"]`);if(el){el.textContent=value;el.closest('button').classList.toggle('alert',alert);}};
      reading('water_tank_status',connected&&!stale?this._text('water_tank_status'):'—',this._state('fill_water')==='on');
      const trayFresh=connected&&!stale,trayMissing=this._state('insert_tray')==='on'||this._state('tray_status')==='Missing',trayFull=this._state('empty_tray')==='on';
      reading('tray_status',trayFresh?(trayMissing?'Missing':trayFull?'Full':this._text('tray_status')):'—',trayFresh&&(trayMissing||trayFull));
      reading('empty_grounds',!connected||stale||!['on','off'].includes(this._state('empty_grounds'))?'—':this._state('insert_coffee_bin')==='on'?'Missing':this._state('empty_grounds')==='on'?'Empty bin':'OK',this._state('empty_grounds')==='on'||this._state('insert_coffee_bin')==='on');
      reading('no_beans',!connected||stale||!['on','off'].includes(this._state('no_beans'))?'—':this._state('no_beans')==='on'?'Refill':'OK',this._state('no_beans')==='on');
      const milkReason=connected&&!stale?this._milkReason('Milk',true):'';
      const milkState=!milkReason?'ok':milkReason==='Milk Container Empty'?'empty':milkReason.startsWith('Milk Cooler Error')?'fault':'no-signal';
      this._card.dataset.milkState=milkState;
      const overrideState=this._state('coolcontrol_override'),overrideOn=overrideState==='on';
      if(this._overridePending===overrideState)this._overridePending=null;
      $('#cooler-override').hidden=!this._entity('coolcontrol_override')||!(overrideOn||milkReason);
      $('#cooler-override').classList.toggle('enabled',overrideOn);
      set('#cooler-override-label',overrideOn?'Milk cooler override is on':'Ignore milk cooler fault');
      set('#cooler-override-help',this._overridePending?'Waiting for the machine to confirm...':!['on','off'].includes(overrideState)?'Override switch unavailable.':overrideOn?'Milk cooler protection is bypassed. Check the milk supply before brewing.':'Allow milk drinks despite the cooler fault. Check the milk supply first.');
      $('#cooler-override-toggle').setAttribute('aria-checked',String(overrideOn));
      $('#cooler-override-toggle').disabled=!!this._overridePending||!['on','off'].includes(overrideState)||!connected||active;

      $('#jura-cooler-warning').setAttribute('display',milkReason?'inline':'none');
      set('#jura-cooler-label',milkState==='empty'?'MILK EMPTY':milkState==='fault'?'COOLER FAULT':milkState==='no-signal'?'NO SIGNAL':'COOL CONTROL');
      $('#jura-cooler-title').textContent=milkReason||'Cool Control';
      if(this._cooler)reading('milk_level',`${this._format(this._state('milk_level'))}% · ${this._format(this._state('milk_temperature'),1)}°C`,!!milkReason);
      const milkReading=$('[data-reading=milk_level]');if(milkReading)milkReading.closest('button').title=milkReason;
      const level=numeric(this._state('milk_level')),height=level===null?0:43*Math.min(100,level)/100;$('#jura-milk-fill').setAttribute('height',height);$('#jura-milk-fill').setAttribute('y',231-height);
      this._card.classList.toggle('tank-alert',connected&&!stale&&this._state('fill_water')==='on');this._card.classList.toggle('tray-alert',connected&&!stale&&(this._state('insert_tray')==='on'||this._state('empty_tray')==='on'));this._card.classList.toggle('beans-alert',connected&&!stale&&this._state('no_beans')==='on');
      this._renderTiles();const selected=this._state('drink');
      if(this._customRecipe&&!this._recipeRun&&!this._selection&&!this._recipeError&&(selected!==this._customRecipe.base_drink||this._state('recipe_modified')==='off'&&Object.entries(this._customRecipe.overrides).some(([field,value])=>String(this._state(recipeKeys[field]))!==String(value)))){this._customRecipe=null;this._recipeError='';}
      if(this._hold&&(active||!connected||stale||status!=='ready'))this._cancelHold();
      const controlsBlocked=!!this._holdBrewPending||!!this._recipeRun||shuttingDown||!connected||status!=='ready'||this._brewLock||pendingCommand(this._state('command_result'));
      this._root.querySelectorAll('[data-drink]').forEach(button=>{button.setAttribute('aria-pressed',String(!this._customRecipe&&button.dataset.drink===selected));const milkReason=this._milkReason(button.dataset.drink);button.disabled=controlsBlocked||!!this._selection||this._edits.size>0||!!milkReason;button.title=milkReason;});
      $('#my-recipes').hidden=!this._recipes.length||this._sections.my_recipes==='hidden';
      this._root.querySelectorAll('[data-recipe]').forEach(button=>{button.setAttribute('aria-pressed',String(Number(button.dataset.recipe)===this._customRecipe?.index));const milkReason=this._milkReason(this._recipes[Number(button.dataset.recipe)]?.base_drink);button.disabled=controlsBlocked||!!this._selection||this._edits.size>0||!!milkReason;button.title=milkReason;});
      this._root.querySelectorAll('[data-count]').forEach(el=>{const state=this._hass.states[this._countEntity(el.dataset.count)]?.state;el.textContent=`${this._format(state)}${el.tagName==='SMALL'?' made':''}`;});
      set('#selected-name',this._customRecipe?.name||(known(selected)?pretty(selected):'Select a drink'));const loading=!!this._recipeRun||!!this._selection||this._state('recipe_loading')==='on';const source=this._text('recipe_source','Unknown');
      set('#recipe-source',loading?'Loading…':source==='Machine'?(this._customRecipe?'Custom recipe':'Machine saved'):source==='Profile defaults'?'Profile defaults':'Source unknown');$('#recipe-source').dataset.fallback=String(source==='Profile defaults'&&!loading);
      set('#modified',this._state('recipe_modified')==='on'?'One-shot changes':'');
      const summary=[];for(const [key,,,label,unit]of numberControls){if(numeric(this._state(key))!==null&&this._bounds(key))summary.push(`${this._format(this._state(key))} ${unit}${key==='water'?'':` ${label.toLowerCase()}`}`);}for(const [key,,,label]of choiceControls){if(known(this._state(key))&&this._choices(key).includes(this._state(key)))summary.push(key==='temperature'?this._state(key):`${label} ${this._state(key)}`);}
      set('#recipe-summary',loading?'Reading saved settings…':summary.join(' · ')||'No supported settings available');
      set('#recipe-message',this._recipeRun?'Loading the saved base recipe and applying your overrides…':loading?'Wait for the machine’s recipe to load.':this._customRecipe?`Based on ${pretty(this._customRecipe.base_drink)}. Unspecified settings use the machine-saved recipe; further edits apply to this brew only.`:source==='Profile defaults'?this._text('recipe_status','Machine settings unavailable; using profile defaults.'):this._state('recipe_modified')==='on'?'Changes apply to the next brew only.':'Uses this drink’s saved machine settings.');
      for(const [key]of choiceControls){const choices=this._choices(key),input=$(`[data-input="${key}"]`),show=choices.length>0&&known(this._state(key));$(`[data-control="${key}"]`).hidden=!show;if(show){const html=choices.map(c=>`<option value="${escapeHTML(c)}">${escapeHTML(c)}</option>`).join('');if(input.innerHTML!==html)input.innerHTML=html;if(this._root.activeElement!==input)input.value=this._state(key);}input.disabled=controlsBlocked||loading||this._edits.has(key);}
      for(const [key,,,label,unit]of numberControls){const bounds=this._bounds(key),input=$(`[data-input="${key}"]`),show=!!bounds&&numeric(this._state(key))!==null;$(`[data-control="${key}"]`).hidden=!show;if(show){input.min=bounds.min;input.max=bounds.max;input.step=bounds.step;if(this._root.activeElement!==input)input.value=this._state(key);set(`[data-bounds="${key}"]`,`${bounds.min}–${bounds.max} ${unit} · step ${bounds.step}`);}input.disabled=controlsBlocked||loading||this._edits.has(key);}
      $('#reset').disabled=controlsBlocked||loading;$('#refresh').disabled=!connected||active;
      const reason=this._holdBrewPending?'Preparing held drink…':shuttingDown?'Preparing to Switch Off':this._brewReason();$('#brew').disabled=!!reason;$('#brew span').textContent=this._brewLock||pendingCommand(this._state('command_result'))?'Preparing drink…':`Brew ${this._customRecipe?.name||(known(selected)?pretty(selected):'drink')}`;set('#brew-reason',reason);
      const command=this._text('command_result','');$('#command').hidden=!command||command==='No command';set('#command',command?`${this._text('requested_product','')}${known(this._state('requested_product'))?' · ':''}${command}`:'');
      $('#acknowledge').hidden=!(this._localUncertain||terminalUncertain(command)&&this._ack!==this._stamp('command_result'));$('#acknowledge').disabled=!connected||status!=='ready'||active||pendingCommand(command);
      $('#error').hidden=!(this._recipeError||this._writeError);set('#error',this._recipeError||this._writeError);
      set('#total',`${this._format(this._state('total_products'))} products`);
      this._root.querySelectorAll('[data-number]').forEach(el=>el.textContent=this._format(this._state(el.dataset.number))+(numeric(this._state(el.dataset.number))!==null?(el.dataset.unit||''):''));
      this._root.querySelectorAll('[data-meter]').forEach(el=>el.style.width=`${Math.min(100,numeric(this._state(el.dataset.meter))||0)}%`);
      this._root.querySelectorAll('[data-diagnostic]').forEach(el=>el.textContent=this._text(el.dataset.diagnostic));
    }
  }
  if(!customElements.get('jura-native-card'))customElements.define('jura-native-card',JuraNativeCard);
  window.customCards=window.customCards||[];if(!window.customCards.some(c=>c.type==='jura-native-card'))window.customCards.push({type:'jura-native-card',name:'Jura Native Graphical Card',description:'Native Jura status, drink selection, saved recipes and observed progress',preview:true});
})();
