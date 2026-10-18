// Jura Native Graphical Card 2.0.8 — native ESPHome entities, no MQTT or external dependencies.
(() => {
  const GRAPHIC = /*GRAPHIC*/;
  const STYLES = /*STYLES*/;
  const escapeHTML = value => String(value ?? '').replace(/[&<>"']/g, ch => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]));
  const known = value => value !== null && value !== undefined && !['','unknown','unavailable','not available'].includes(String(value).trim().toLowerCase());
  const numeric = value => known(value) && Number.isFinite(Number(value)) && Number(value) >= 0 ? Number(value) : null;
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
  "error milk (milk sensor)": "Milk Sensor Error",
  "no signal (milk sensor)": "Milk Sensor Not Responding",
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
  class JuraNativeCard extends HTMLElement {
    constructor(){ super();this.attachShadow({mode:'open'});this._timers=new Set();this._edits=new Map();this._selection=null;this._brewLock=false;this._writeError=''; }
    setConfig(config){
      this._config={entity_prefix:'jura_e8',name:'Jura E8',...config};
      const p=this._config.entity_prefix,sensor=suffix=>`sensor.${p}_${suffix}`,binary=suffix=>`binary_sensor.${p}_${suffix}`;
      const textKeys=['status','status_detail','active_alerts','current_product','progress_state','stage_unit','command_result','requested_product','recipe_source','recipe_status','saved_recipe_raw','command_recipe_source','command_recipe_status','command_recipe_raw','session','profile','tf_raw','tv_raw','progress','stage_current','stage_target','product_code','stage_code','total_products','uart_timeouts','uart_framing_errors'];
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
      Object.assign(this._entities,config.entities||{});
      this._signature=null;this._build();this._update();
    }
    set hass(hass){this._hass=hass;this._update();}
    getCardSize(){return 15;}
    getGridOptions(){return {columns:12,min_columns:6};}
    static getStubConfig(){return {name:'Jura E8',entity_prefix:'jura_e8'};}
    disconnectedCallback(){for(const id of this._timers)clearTimeout(id);this._timers.clear();}
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
       <section class="drink-section" aria-label="Drink selection"><div class="section-label">CHOOSE A DRINK<span>Select, then brew</span></div><div id="drink-tiles" class="drink-tiles"></div><details id="more-drinks"><summary>More drinks<span id="more-count"></span></summary><div id="extra-tiles" class="drink-tiles"></div></details></section>
       <section id="recipe" class="recipe" aria-label="Selected drink recipe"><div class="recipe-heading"><strong id="selected-name">Select a drink</strong><span id="recipe-source" class="source-chip"></span></div><div id="recipe-summary"></div><div id="recipe-message" class="recipe-message"></div><details id="adjust"><summary>Adjust this drink<span id="modified"></span></summary><div class="recipe-controls">${choiceControls.map(([key,,,label])=>`<label data-control="${key}" hidden>${label}<select data-input="${key}" aria-label="${label}"></select></label>`).join('')}${numberControls.map(([key,,,label,unit])=>`<label data-control="${key}" hidden>${label} <span class="unit">${unit}</span><input type="number" data-input="${key}" aria-label="${label}" inputmode="numeric"><span class="bounds" data-bounds="${key}"></span></label>`).join('')}</div><button id="reset" class="text-button" type="button">Reset to saved recipe</button></details><button id="brew" class="brew-button" type="button" disabled><ha-icon icon="mdi:coffee-maker"></ha-icon><span>Brew</span></button><div id="brew-reason" class="brew-reason"></div><div id="command" class="command" role="status" hidden></div><button id="acknowledge" class="text-button" type="button" hidden>I checked the machine</button><div id="error" class="error" role="alert" hidden></div></section>
       <details id="maintenance"><summary><span><ha-icon icon="mdi:wrench-outline"></ha-icon>Maintenance & totals</span><span id="total"></span></summary><div class="maintenance-meters">${maintenance.slice(0,3).map(([key,label])=>`<button class="maintenance-meter" data-info="${key}_maintenance"><span>${label}</span><strong data-number="${key}_maintenance" data-unit="%">—</strong><div class="meter-track"><div data-meter="${key}_maintenance"></div></div></button>`).join('')}</div><div class="detail-rows">${maintenance.map(([key,label])=>`<button data-info="${key}_count" class="detail-row"><span>${label} count</span><strong data-number="${key}_count">—</strong></button>`).join('')}</div><div id="all-counters" class="detail-rows"></div><button id="refresh" class="text-button">Refresh readings</button></details>
       <details id="diagnostics"><summary><span><ha-icon icon="mdi:pulse"></ha-icon>Diagnostics</span></summary>${['detected_model','profile','session','current_product','progress_state','stage_code','requested_product','command_result','command_recipe_source','command_recipe_status','recipe_status','saved_recipe_raw','command_recipe_raw','tf_raw','tv_raw','uart_timeouts','uart_framing_errors','grounds_level','changed_counters'].map(key=>`<button class="diagnostic-row" data-info="${key}"><span>${key.replaceAll('_',' ')}</span><code data-diagnostic="${key}">—</code></button>`).join('')}<div class="diagnostic-note">Legacy grounds and raw counters are unverified. They are not used for drink totals or readiness.</div></details>
      </ha-card>`;
      this._root=this.shadowRoot;this._card=this._root.querySelector('ha-card');
      this._root.addEventListener('click',event=>{const node=event.target.closest('[data-info]');if(node)this._info(node.dataset.info);});
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
      this._root.querySelector('#more-drinks').hidden=!extra.length;this._root.querySelector('#more-count').textContent=extra.length?String(extra.length):'';
      this._root.querySelectorAll('[data-drink]').forEach(button=>button.addEventListener('click',()=>this._selectDrink(button.dataset.drink)));
      this._root.querySelector('#all-counters').innerHTML=options.map(name=>`<button class="detail-row" data-counter-info="${escapeHTML(name)}"><span>${escapeHTML(pretty(name))}</span><strong data-count="${escapeHTML(name)}">—</strong></button>`).join('');
      this._root.querySelectorAll('[data-counter-info]').forEach(button=>button.addEventListener('click',()=>{const id=this._countEntity(button.dataset.counterInfo);if(this._hass.states[id])this.dispatchEvent(new CustomEvent('hass-more-info',{detail:{entityId:id},bubbles:true,composed:true}));}));
    }
    async _selectDrink(name){
      if(!this._options().includes(name)||this._selection||this._brewLock||this._state('connected')!=='on'||this._state('status')!=='ready'||pendingCommand(this._state('command_result')))return;
      if(name===this._state('drink'))return;
      this._writeError='';this._selection={name,sawLoading:false,recipeStamp:this._stamp('recipe_status')};this._update(true);
      if(!await this._service('select','select_option','drink',{option:name})){this._selection=null;this._update(true);return;}
      this._selectionTimeout();
    }
    _selectionTimeout(){const selection=this._selection;this._later(()=>{if(this._selection===selection){this._selection=null;this._editUnconfirmed=true;this._writeError='Recipe change unconfirmed. Reset the recipe before brewing.';this._update(true);}},15000);}
    async _reset(){
      if(this._state('connected')!=='on'||this._state('status')!=='ready'||pendingCommand(this._state('command_result'))||this._brewLock)return;
      this._writeError='';this._editUnconfirmed=false;this._edits.clear();this._selection={name:this._state('drink'),sawLoading:false,recipeStamp:this._stamp('recipe_status')};this._update(true);
      if(!await this._service('button','press','reset_recipe')){this._selection=null;this._editUnconfirmed=true;this._update(true);return;}this._selectionTimeout();
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
    _brewReason(){
      if(this._state('connected')!=='on')return 'Machine disconnected';
      if(this._state('brew_active')==='on')return 'Machine operation in progress';
      if(this._state('status')!=='ready')return friendlyAlert(this._text('status_detail','Waiting for the machine to be ready'));
      if(this._state('alerts_stale')!=='off')return 'Waiting for fresh machine alerts';
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
    async _brew(){
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
        if(this._state('drink')===this._selection.name&&this._state('recipe_loading')==='off'&&(this._selection.sawLoading||this._text('recipe_status','')===`Saved recipe loaded for ${this._selection.name}`||this._stamp('recipe_status')!==this._selection.recipeStamp&&/^Profile defaults:/i.test(this._text('recipe_status',''))))this._selection=null;
      }
      for(const [key,expected]of this._edits){if(String(this._state(key))===expected||numeric(this._state(key))!==null&&String(Number(this._state(key)))===expected)this._edits.delete(key);}
      const observedProduct=this._text('current_product','');
      if(!active||this._phaseProduct!==observedProduct||/grinding/i.test(phase))this._coffeeBeforeMilk=false;
      if(active&&phase==='Dispensing water (milk drink)')this._coffeeBeforeMilk=true;
      this._phaseProduct=observedProduct;
      const displayPhase=friendlyStage(phase,observedProduct,this._coffeeBeforeMilk);
      const mode=!connected?(status==='connecting'?'connecting':'offline'):shuttingDown?'switching_off':status==='blocked'?'blocked':status==='heating'?'heating':active?'busy':status;
      this._card.dataset.mode=mode;this._card.dataset.phase=active?(code===33?'heating':code===49||code===57?'grinding':code===50?'milk':code===52?'foam':code===51?'pause':code===55||code===60?'coffee':code===65?'water':/rins|clean|descal/i.test(phase)?'rinse':'working'):mode==='heating'?'heating':finished?'finished':'idle';
      const statusNames={switching_off:'Switching off',ready:'Ready',busy:'Making a drink',blocked:'Needs attention',heating:'Heating',finishing:'Finishing',not_ready:'Not ready',connecting:'Connecting',offline:'Offline',unknown:'Unknown'};
      set('#status',statusNames[mode]||'Unknown');set('#detail',shuttingDown?'Preparing to Switch Off':!connected&&status!=='connecting'?'Machine disconnected':active?displayPhase||'Machine operation in progress':friendlyAlert(this._text('status_detail','Waiting for machine status')));
      const alerts=[];if(activeAlerts&&activeAlerts.toLowerCase()!=='none')alerts.push(friendlyAlert(activeAlerts));if(stale&&!/stale/i.test(activeAlerts))alerts.push('Alert readings are stale');
      $('#alerts').hidden=!connected||!alerts.length;set('#alerts',connected?alerts.join(' · '):'');
      const freshReady=connected&&!stale&&status==='ready';
      const operation=connected&&!shuttingDown&&!freshReady&&(active||finished||/progress timeout/i.test(phase));$('#operation').hidden=!operation;
      set('#actual-product',pretty(this._text('current_product','Machine operation')));set('#phase',displayPhase||'Waiting for progress');
      const overall=numeric(this._state('progress'));const percent=overall!==null&&overall<=100?overall:null;
      set('#overall',percent===null?'Progress unavailable':`${this._format(percent)}%`);
      $('#progress-track').hidden=percent===null;$('#progress-track').setAttribute('aria-valuenow',percent===null?'0':String(percent));$('#progress-fill').style.width=`${percent||0}%`;
      const current=numeric(this._state('stage_current')),target=numeric(this._state('stage_target')),unit=this._text('stage_unit','');
      set('#stage',(current!==null||target!==null)&&['ml','s','level'].includes(unit)?`${this._format(current)} / ${this._format(target)} ${unit}`:'');
      const screen=shuttingDown?'Switching Off':connected?(active?(/grind/i.test(phase)?'Grinding':/brewing/i.test(displayPhase)?'Brewing':phase==='Dispensing milk foam'?'Adding Foam':phase==='Dispensing milk'?'Adding Milk':/heating/i.test(phase)?'Heating':phase==='Milk pause'?'Settling Milk':phase==='Dispensing water'?'Hot Water':displayPhase.slice(0,16)):statusNames[status]||'Unknown'):'Offline';
      set('#jura-screen-status',screen);
      const reading=(key,value,alert=false)=>{const el=$(`[data-reading="${key}"]`);if(el){el.textContent=value;el.closest('button').classList.toggle('alert',alert);}};
      reading('water_tank_status',connected&&!stale?this._text('water_tank_status'):'—',this._state('fill_water')==='on');
      reading('tray_status',connected&&!stale?this._text('tray_status'):'—',this._state('insert_tray')==='on'||this._state('empty_tray')==='on');
      reading('empty_grounds',!connected||stale||!['on','off'].includes(this._state('empty_grounds'))?'—':this._state('insert_coffee_bin')==='on'?'Missing':this._state('empty_grounds')==='on'?'Empty bin':'OK',this._state('empty_grounds')==='on'||this._state('insert_coffee_bin')==='on');
      reading('no_beans',!connected||stale||!['on','off'].includes(this._state('no_beans'))?'—':this._state('no_beans')==='on'?'Refill':'OK',this._state('no_beans')==='on');
      if(this._cooler)reading('milk_level',`${this._format(this._state('milk_level'))}% · ${this._format(this._state('milk_temperature'),1)}°C`);
      const level=numeric(this._state('milk_level')),height=level===null?0:43*Math.min(100,level)/100;$('#jura-milk-fill').setAttribute('height',height);$('#jura-milk-fill').setAttribute('y',231-height);
      this._card.classList.toggle('tank-alert',connected&&!stale&&this._state('fill_water')==='on');this._card.classList.toggle('tray-alert',connected&&!stale&&(this._state('insert_tray')==='on'||this._state('empty_tray')==='on'));this._card.classList.toggle('beans-alert',connected&&!stale&&this._state('no_beans')==='on');
      this._renderTiles();const selected=this._state('drink');
      const controlsBlocked=shuttingDown||!connected||status!=='ready'||this._brewLock||pendingCommand(this._state('command_result'));
      this._root.querySelectorAll('[data-drink]').forEach(button=>{button.setAttribute('aria-pressed',String(button.dataset.drink===selected));button.disabled=controlsBlocked||!!this._selection||this._edits.size>0;});
      this._root.querySelectorAll('[data-count]').forEach(el=>{const state=this._hass.states[this._countEntity(el.dataset.count)]?.state;el.textContent=`${this._format(state)}${el.tagName==='SMALL'?' made':''}`;});
      set('#selected-name',known(selected)?pretty(selected):'Select a drink');const loading=!!this._selection||this._state('recipe_loading')==='on';const source=this._text('recipe_source','Unknown');
      set('#recipe-source',loading?'Loading…':source==='Machine'?'Machine saved':source==='Profile defaults'?'Profile defaults':'Source unknown');$('#recipe-source').dataset.fallback=String(source==='Profile defaults'&&!loading);
      set('#modified',this._state('recipe_modified')==='on'?'One-shot changes':'');
      const summary=[];for(const [key,,,label,unit]of numberControls){if(numeric(this._state(key))!==null&&this._bounds(key))summary.push(`${this._format(this._state(key))} ${unit}${key==='water'?'':` ${label.toLowerCase()}`}`);}for(const [key,,,label]of choiceControls){if(known(this._state(key))&&this._choices(key).includes(this._state(key)))summary.push(key==='temperature'?this._state(key):`${label} ${this._state(key)}`);}
      set('#recipe-summary',loading?'Reading saved settings…':summary.join(' · ')||'No supported settings available');
      set('#recipe-message',loading?'Wait for the machine’s recipe to load.':source==='Profile defaults'?this._text('recipe_status','Machine settings unavailable; using profile defaults.'):this._state('recipe_modified')==='on'?'Changes apply to the next brew only.':'Uses this drink’s saved machine settings.');
      for(const [key]of choiceControls){const choices=this._choices(key),input=$(`[data-input="${key}"]`),show=choices.length>0&&known(this._state(key));$(`[data-control="${key}"]`).hidden=!show;if(show){const html=choices.map(c=>`<option value="${escapeHTML(c)}">${escapeHTML(c)}</option>`).join('');if(input.innerHTML!==html)input.innerHTML=html;if(this._root.activeElement!==input)input.value=this._state(key);}input.disabled=controlsBlocked||loading||this._edits.has(key);}
      for(const [key,,,label,unit]of numberControls){const bounds=this._bounds(key),input=$(`[data-input="${key}"]`),show=!!bounds&&numeric(this._state(key))!==null;$(`[data-control="${key}"]`).hidden=!show;if(show){input.min=bounds.min;input.max=bounds.max;input.step=bounds.step;if(this._root.activeElement!==input)input.value=this._state(key);set(`[data-bounds="${key}"]`,`${bounds.min}–${bounds.max} ${unit} · step ${bounds.step}`);}input.disabled=controlsBlocked||loading||this._edits.has(key);}
      $('#reset').disabled=controlsBlocked||loading;$('#refresh').disabled=!connected||active;
      const reason=shuttingDown?'Preparing to Switch Off':this._brewReason();$('#brew').disabled=!!reason;$('#brew span').textContent=this._brewLock||pendingCommand(this._state('command_result'))?'Preparing drink…':`Brew ${known(selected)?pretty(selected):'drink'}`;set('#brew-reason',reason);
      const command=this._text('command_result','');$('#command').hidden=!command||command==='No command';set('#command',command?`${this._text('requested_product','')}${known(this._state('requested_product'))?' · ':''}${command}`:'');
      $('#acknowledge').hidden=!(this._localUncertain||terminalUncertain(command)&&this._ack!==this._stamp('command_result'));$('#acknowledge').disabled=!connected||status!=='ready'||active||pendingCommand(command);
      $('#error').hidden=!this._writeError;set('#error',this._writeError);
      set('#total',`${this._format(this._state('total_products'))} products`);
      this._root.querySelectorAll('[data-number]').forEach(el=>el.textContent=this._format(this._state(el.dataset.number))+(numeric(this._state(el.dataset.number))!==null?(el.dataset.unit||''):''));
      this._root.querySelectorAll('[data-meter]').forEach(el=>el.style.width=`${Math.min(100,numeric(this._state(el.dataset.meter))||0)}%`);
      this._root.querySelectorAll('[data-diagnostic]').forEach(el=>el.textContent=this._text(el.dataset.diagnostic));
    }
  }
  if(!customElements.get('jura-native-card'))customElements.define('jura-native-card',JuraNativeCard);
  window.customCards=window.customCards||[];if(!window.customCards.some(c=>c.type==='jura-native-card'))window.customCards.push({type:'jura-native-card',name:'Jura Native Graphical Card',description:'Native Jura status, drink selection, saved recipes and observed progress',preview:true});
})();
