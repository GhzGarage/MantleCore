-- Mantle scripting API for Luau. GENERATED from the engine's API registry; do not edit.
-- Use with luau-lsp: set "luau-lsp.types.definitionFiles" to this file and "luau-lsp.platform.type" to "standard".

export type Signal<T... = ...any> = {
	Connect: (self: Signal<T...>, handler: (T...) -> ()) -> Connection,
	Once: (self: Signal<T...>, handler: (T...) -> ()) -> Connection,
	Wait: (self: Signal<T...>) -> T...,
	--- Fire a custom event from Events:Get.
	Fire: (self: Signal<T...>, T...) -> (),
}

declare extern type Vector3 with
	X: number
	Y: number
	Z: number
	Magnitude: number
	Unit: Vector3
	function Dot(self, other: Vector3): number
	function Cross(self, other: Vector3): Vector3
	function Lerp(self, goal: Vector3, alpha: number): Vector3
	function Distance(self, other: Vector3): number
	function __add(self, other: Vector3 | number): Vector3
	function __sub(self, other: Vector3 | number): Vector3
	function __mul(self, other: Vector3 | number): Vector3
	function __div(self, other: Vector3 | number): Vector3
	function __unm(self): Vector3
end

declare Vector3: {
	new: (x: number?, y: number?, z: number?) -> Vector3,
	zero: Vector3,
	one: Vector3,
	xAxis: Vector3,
	yAxis: Vector3,
	zAxis: Vector3,
}

--- A thing in the world: any actor in the level.
declare extern type Entity with
	--- Whether a physics entity is held where it is (true) or simulating (false); the place's physics.simulate says how it starts. Set it on the server to drop or pin it. Every other entity is always anchored, and setting it on one errors.
	Anchored: boolean
	--- How fast the entity spins, in degrees per second about the world axes (X, Y, Z). Setting it (server only) replaces a physics entity's spin; setting it on an Anchored entity, or one with no physics block, errors. Anything that isn't simulating reads (0, 0, 0).
	AngularVelocity: Vector3
	--- Whether players and objects bump into it.
	CanCollide: boolean
	--- How many seconds into its clip a SkeletalMesh is now, the same on every screen; nil when it plays no clip, while the clip is still downloading, and always on a dedicated server (it never loads clips). Read-only. Errors when the entity is not a SkeletalMesh.
	ClipTime: number?
	--- A mesh's tint as "#RRGGBB", multiplied into its materials; "#FFFFFF" is none. Setting it on the server changes it for everyone, players who join later included; on a client it only changes that client's view. Only materials uploaded with the place can be tinted: errors on an entity with none of those, or that is not a mesh.
	Color: string
	--- False once the entity has been destroyed.
	Exists: boolean
	--- The light's colour as "#RRGGBB", the same format as Vehicles:SetPaintColor. Replicates like LightEnabled.
	LightColor: string
	--- Whether the light shines. Setting it on the server changes it for everyone, players who join later included; on a client it only changes that client's view. Errors if the entity is not a light.
	LightEnabled: boolean
	--- A spot light's fully lit cone, in degrees, clamped to 0..90. Errors on other lights. Replicates like LightEnabled.
	LightInnerCone: number
	--- How bright the light is, in its own units: a place light keeps the units it was made with, a Lights:Spawn light uses candelas, and a directional light uses lux. Clamped to 0..100000 (0..200 for a directional light). Replicates like LightEnabled.
	LightIntensity: number
	--- "Point", "Spot" or "Directional" when the entity is a light, nil for anything else. Read-only.
	LightKind: string?
	--- A spot light's outer cone, where its light fades to nothing, in degrees, clamped to 0..90. Errors on other lights. Replicates like LightEnabled.
	LightOuterCone: number
	--- How far a point or spot light reaches, in centimetres, clamped to 1..100000. Errors on a directional light. Replicates like LightEnabled.
	LightRange: number
	--- A physics entity's mass in kg: the place's physics.mass, else computed from its collision. 0 for an entity with no physics block. Read-only.
	Mass: number
	--- The actor's label.
	Name: string
	--- A physics entity's surface: "default", "rubber" (bouncy), "ice" (slippery), "metal", "wood" or "stone"; the place's physics.material says how it starts. Set it on the server, anchored or not, and every player sees the change. nil for an entity with no physics block, and setting it on one errors, as does any other name.
	PhysicsMaterial: string?
	--- World location in centimetres. Setting it on the server replicates to everyone; on a client it only changes that client's view. Setting it (or Rotation or Scale) on an entity the place did not mark dynamic is a script error; clones are always dynamic.
	Position: Vector3
	--- Rotation in degrees as (pitch, yaw, roll).
	Rotation: Vector3
	--- Scale (1, 1, 1 is normal size).
	Scale: Vector3
	--- The id of a sound emitter's sound, to pass to Sounds:Play or Sounds:PlayMusic; nil for any other entity.
	SoundId: string?
	--- Sound emitters only: the emitter's volume, 0 to 2. Setting it from a server script changes it for every player.
	SoundVolume: number
	--- How see-through a mesh is, from 0 (solid) to 1 (invisible); values outside are clamped. Replicates like Color. Only materials uploaded with the place can be made see-through: errors on an entity with none of those.
	Transparency: number
	--- How fast the entity moves, in cm/s. Setting it (server only) replaces a physics entity's velocity, and does nothing while it is Anchored; setting it on an entity with no physics block errors.
	Velocity: Vector3
	--- Whether it is drawn. Hidden entities still collide unless CanCollide is false.
	Visible: boolean
	--- An attribute on this entity changed.
	AttributeChanged: Signal<(string, any)>
	--- A physics entity started touching something (server only): another entity, a player's character, or nil for anything else. Fires once when a contact starts, not while it lasts, and at most a few dozen times a tick across the place. Only physics entities fire it.
	Collided: Signal<(Entity?)>
	--- A player's character stopped touching this entity.
	TouchEnded: Signal<(Player)>
	--- A player's character started touching this entity.
	Touched: Signal<(Player)>
	--- Give the entity a tag (fires World:OnTagged handlers). (server only)
	function AddTag(self, tag: string): ()
	--- Spin a physics entity: an angular impulse in kg*cm^2/s about the world axes, in degrees. Does nothing while it is Anchored. Errors on an entity with no physics block. (server only)
	function ApplyAngularImpulse(self, impulse: Vector3): ()
	--- Push a physics entity: impulse is in kg*cm/s along the world axes, so its velocity changes by impulse / Mass. Does nothing while it is Anchored. Errors on an entity with no physics block. (server only)
	function ApplyImpulse(self, impulse: Vector3): ()
	--- Make a copy (same look, tags and place). On the server the copy appears for everyone; on a client only for that player. Tip: keep a hidden template and clone it. A physics entity, a Constraint or an NPC can't be cloned (it errors, and says how to make another).
	function Clone(self): Entity
	--- Remove the entity. On the server it disappears for everyone; on a client only for that player.
	function Destroy(self): ()
	--- A custom value stored on the entity, or nil.
	function GetAttribute(self, name: string): any
	--- The material asset id a mesh shows in a slot (0 when left out), or nil when the slot shows the mesh's own material.
	function GetMaterial(self, slot: number?): string?
	--- All of the entity's tags.
	function GetTags(self): {string}
	--- Whether the entity has this tag.
	function HasTag(self, tag: string): boolean
	--- Call handler with the attribute's value now and every time it changes.
	function ObserveAttribute(self, name: string, handler: (value: any) -> ()): Connection
	--- Play one of the place's AnimClip assets on a SkeletalMesh, starting now. Leave clipAssetId out (or nil) to restart the clip it plays now, or its own. params: Loop (true by default; false holds the last frame), PlayRate (clip seconds per second, 1 by default, clamped to 0..10) and StartOffset (seconds into the clip to start at, 0 by default, clamped to 0..3600). On the server it plays for everyone, players who join later included, in step on every screen; on a client it only changes that client's view. Errors when the entity is not a SkeletalMesh, the id is not an AnimClip in the place's assets, or the clip was authored on another skeleton (on a dedicated server, which never loads clips, that last one shows on each player's screen as the mesh's resting pose instead).
	function PlayClip(self, clipAssetId: string?, params: {Loop: boolean?, PlayRate: number?, StartOffset: number?}?): ()
	--- Effects only: start emitting. A looping effect already playing keeps going; one that does not loop bursts for a second again. On the server it plays for everyone, players who join later included; on a client it only changes that client's view. Errors when the entity is not an Effect.
	function PlayEffect(self): ()
	--- Sound emitters only: play the emitter's sound again from the beginning (looping if the emitter loops). From a server script every player hears it.
	function PlaySound(self): ()
	--- Remove a tag (fires World:OnUntagged handlers). (server only)
	function RemoveTag(self, tag: string): ()
	--- Store a custom value on the entity (numbers, strings, booleans, Vector3, tables). Server values replicate.
	function SetAttribute(self, name: string, value: any): ()
	--- Effects only: tint the particles, "#RRGGBB"; the next particles take it. Replicates like PlayEffect. Errors when the entity is not an Effect or the colour is not "#RRGGBB".
	function SetEffectColor(self, color: string): ()
	--- Effects only: how many particles spawn a second, clamped to 0..200; 0 emits nothing while it still plays. Replicates like PlayEffect. Errors when the entity is not an Effect.
	function SetEffectRate(self, rate: number): ()
	--- Show another of the place's Material assets on this mesh: in one material slot (counting from 0), or in every slot when slot is left out. "" puts the place's own material back. Replicates like Color. Errors when the id is not a Material in the place's assets, the slot doesn't exist, or the entity is not a mesh.
	function SetMaterial(self, assetId: string, slot: number?): ()
	--- Stop a SkeletalMesh's clip: it shows its resting pose until the next PlayClip. Replicates like PlayClip. Errors when the entity is not a SkeletalMesh.
	function StopClip(self): ()
	--- Effects only: stop emitting; the particles already out die out. Replicates like PlayEffect. Errors when the entity is not an Effect.
	function StopEffect(self): ()
	--- Sound emitters only: stop the emitter's sound, fading it out over fadeOut seconds (0 by default). It stays silent until PlaySound, from a server script for players who join later too.
	function StopSound(self, fadeOut: number?): ()
	--- Smoothly move/rotate/scale to the goal. Since script API 1.8 the goal can also fade a light's LightColor or LightIntensity, or a mesh's Color; those errors like setting them would. style: Linear, Sine, Quad, Cubic, Quart, Quint, Exponential, Circular, Back, Bounce, Elastic. direction: In, Out, InOut. Returns a Completed signal. A Position, Rotation or Scale goal on an entity the place did not mark dynamic is a script error, like setting them.
	function Tween(self, goal: {Position: Vector3?, Rotation: Vector3?, Scale: Vector3?, LightColor: string?, LightIntensity: number?, Color: string?}, seconds: number, style: string?, direction: string?): Signal
end

--- A connected player.
declare extern type Player with
	--- How much of every hit the character shrugs off, as a percent from 0 (none, the default) to 100 (all of it). Armor works before the Shield and health. nil while the player has no character. Setting it (server only) clamps to 0..100 and lasts for the rest of the session, through respawns.
	Armor: number?
	--- The item id of the avatar (body) the player has on, or nil before their look has arrived. Read-only. (server only)
	Avatar: string?
	--- The player's body in the world, or nil while respawning.
	Character: Entity?
	--- Whether the player can crouch or slide (true unless a script changes it); nil while the player has no character. Setting it (server only) to false stands a crouching player up and the crouch key does nothing until it is true again. It lasts for the rest of the session, through respawns.
	CrouchEnabled: boolean?
	--- How fast the character moves while crouched, in centimetres per second (225 unless a script changes it; it scales with WalkSpeed until you set it). nil while the player has no character. Setting it (server only) clamps to 0..1500 and lasts for the rest of the session, through respawns.
	CrouchSpeed: number?
	--- The character's health, from 0 to MaxHealth; nil while the player has no character. Weapons and TakeDamage lower this same value. Setting it (server only) clamps to 0..MaxHealth, and 0 kills the character with no killer. Setting it on a dead player, or one driving a vehicle, does nothing: use Respawn to bring a player back.
	Health: number?
	--- Whether the character is crouched right now (by the key, a slide or ForceCrouch). nil while the player has no character. Read-only.
	IsCrouched: boolean?
	--- Whether the character is limp on the ground right now: from Ragdoll, or a hard hit, a blast or being thrown off a vehicle knocked it down. nil while the player has no character. Read-only.
	IsRagdolled: boolean?
	--- Whether the character is sprinting right now: holding the sprint key and moving forward fast enough. nil while the player has no character. Read-only.
	IsSprinting: boolean?
	--- How high the character's jump rises, in centimetres (about 90 under normal gravity unless a script changes it). nil while the player has no character. Setting it (server only) clamps to 0..1000 and lasts for the rest of the session, through respawns; 0 stops ordinary jumps.
	JumpHeight: number?
	--- How much stamina a jump costs (0 unless a script changes it, so jumping is free); with a cost, a character with less stamina than that cannot jump. nil while the player has no character. Setting it (server only) clamps to 0..10000 and lasts for the rest of the session. Only counts while StaminaEnabled is true.
	JumpStaminaCost: number?
	--- The most health the character can have (100 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 1..10000 and lasts for the rest of the session: every respawn starts with full health at this maximum. Lowering it below Health lowers Health too; raising it does not heal.
	MaxHealth: number?
	--- The size of the shield pool (0, no shield, unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..10000 and lasts for the rest of the session. A shield above the new size drops to it; raising an existing shield does not fill the pool (set Shield for that), but giving a shield to a player who had none starts it full.
	MaxShield: number?
	--- The most stamina the character can have (100 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 1..10000 and lasts for the rest of the session, through respawns. Lowering it below Stamina lowers Stamina too; raising it does not refill.
	MaxStamina: number?
	--- The player's Epic display name.
	Name: string
	--- Where the player's character is.
	Position: Vector3?
	--- Seconds the character must go without being hurt before health and shield start coming back (5 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..60 and lasts for the rest of the session.
	RegenDelay: number?
	--- Health the character gets back per second once RegenDelay seconds have passed since it was last hurt (0, no regeneration, unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..1000 and lasts for the rest of the session.
	RegenRate: number?
	--- The character's shield: a pool that soaks up damage (after Armor) before health is touched. 0 to MaxShield; nil while the player has no character. Setting it (server only) changes the pool now and clamps to 0..MaxShield; every respawn starts with it full.
	Shield: number?
	--- Shield the character gets back per second after the same RegenDelay (0, none, unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..1000 and lasts for the rest of the session.
	ShieldRegenRate: number?
	--- Whether the player can sprint (true unless a script changes it); nil while the player has no character. Setting it (server only) to false stops a sprint at once and the sprint key does nothing until it is true again. It lasts for the rest of the session, through respawns.
	SprintEnabled: boolean?
	--- How fast the character sprints, in centimetres per second (700 unless a script changes it; it scales with WalkSpeed until you set it). nil while the player has no character. Setting it (server only) clamps to 0..1500 and lasts for the rest of the session, through respawns.
	SprintSpeed: number?
	--- The stamina the character has left, from 0 to MaxStamina (it counts in small steps, the same on every machine, and moves only while StaminaEnabled is true); nil while the player has no character. Setting it (server only) clamps to 0..MaxStamina. Every new character starts full.
	Stamina: number?
	--- How much stamina sprinting uses each second (20 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..10000 and lasts for the rest of the session.
	StaminaDrain: number?
	--- Whether the player has a stamina pool (false unless a script turns it on, so places that never use it play as before); nil while the player has no character. With it on, sprinting drains Stamina, Stamina refills after a short rest, a sprint stops at zero and cannot restart until a tenth of MaxStamina has come back, and jumping can cost JumpStaminaCost. Turning it on starts the pool full. Setting it (server only) lasts for the rest of the session, through respawns.
	StaminaEnabled: boolean?
	--- How much stamina comes back each second once the character has rested (15 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..10000 and lasts for the rest of the session.
	StaminaRegen: number?
	--- How many seconds after sprinting (or a jump that cost stamina) before stamina starts coming back (1 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..60 and lasts for the rest of the session.
	StaminaRegenDelay: number?
	--- The name of the team the player is on, or nil for none. Setting it (server only) puts the player on that team, which must exist (Teams:Create); set nil to take them off. Fires Teams.PlayerTeamChanged. Team membership replicates, so client scripts can read it, and it lasts while the player is in the session.
	Team: string?
	--- The player's verified Mantle account id (a plain UUID).
	UserId: string
	--- How fast the character runs, in centimetres per second (500 unless a script changes it); walking, sprinting and crouching scale with it. nil while the player has no character. Setting it (server only) clamps to 0..1500 and lasts for the rest of the session, through respawns; 0 stops the character moving.
	WalkSpeed: number?
	--- What the player is wearing: slot name ("hair", "top", "footwear", "acc_hat", ...) to item id. Empty when nothing is worn or their look has not arrived. Read-only; a fresh table on every read. (server only)
	Wearing: {[string]: string}
	--- The character crouched (the key, a slide or ForceCrouch). Fires on the server and on every client.
	CrouchStarted: Signal<(Player)>
	--- The character stood up from a crouch. Fires on the server and on every client.
	CrouchStopped: Signal<(Player)>
	--- The character was hurt, on the server. amount is what got through Armor (the Shield and health took it between them), so a hit the shield soaked up still counts; a killing hit fires Damaged first and then Died. damageType is the hit's tag ("Bullet", "Melee", "Explosion", ..., or your own) and attacker the player who dealt it, or nil. Not fired for a hit a held guard blocked, friendly fire that was switched off, or Health set directly.
	Damaged: Signal<(Player, number, string, Player?)>
	--- The player's character died, on the server and on every client. killer is the player whose damage finished it (a weapon, or TakeDamage with a source); nil for falling out of the world, Kill, Health set to 0, or their own damage. damageType is the tag of the killing hit ("Bullet", "Melee", "Explosion", ..., or your own); "Generic" when nothing named one.
	Died: Signal<(Player, Player?, string)>
	--- The character's health changed: damage, healing, Health set, or a respawn's refill. Fires on the server and on every client.
	HealthChanged: Signal<(Player, number, number)>
	--- The character was knocked down (Ragdoll, a hard hit, a blast, or being thrown off a vehicle). Fires on the server and on every client.
	Ragdolled: Signal<(Player)>
	--- A knocked-down character stood back up (its time ended or Recover was called). It does not fire if the character died while down: Died does. Fires on the server and on every client.
	Recovered: Signal<(Player)>
	--- The player got a fresh character after a death or a Respawn call, on the server. Not fired for the first character a player gets when they join.
	Respawned: Signal<(Player)>
	--- The character started sprinting. Fires on the server and on every client.
	SprintStarted: Signal<(Player)>
	--- The character stopped sprinting: the key let go, stamina ran out, SprintEnabled turned off, or it stopped moving. Fires on the server and on every client.
	SprintStopped: Signal<(Player)>
	--- The character's Stamina changed: sprinting, resting, a jump's cost or a script setting it. It reports steps of about half a percent of MaxStamina, not every frame. Fires on the server and on every client while StaminaEnabled is true.
	StaminaChanged: Signal<(Player, number, number)>
	--- Hold the player crouched (true) or standing (false) whatever they press, until ReleaseCrouch. A player held crouched can still walk (sprinting is off while crouched); one held standing cannot crouch or slide. CrouchEnabled false wins over a crouch hold. Returns false when the player has no character on foot. The hold ends with the character: a respawn gives the key back. (server only)
	function ForceCrouch(self, crouched: boolean): boolean
	--- Raise the character's health by amount, up to MaxHealth. Never brings a dead character back (use Respawn). Returns false when the player has no character on foot, is dead, or is already at full health. (server only)
	function Heal(self, amount: number): boolean
	--- Remove the player from the session with a message. (server only)
	function Kick(self, reason: string?): ()
	--- Kill the character outright: Died fires with no killer, and the place's respawn rules take over as for any other death. Returns false when the player has no character on foot or is already dead. (server only)
	function Kill(self): boolean
	--- Knock the character down: it goes limp for everyone, the player cannot move or act, and when the time is up (or Recover is called) it stands back up where it lies with a get-up animation. seconds is how long to stay down, 0.1 to 300; leave it out to stay down until Recover (at most five minutes). velocity is an optional shove in centimetres per second; without it the character keeps the speed it had. Ragdolled fires now and Recovered when it gets up. Returns false when the player has no character on foot, is dead, is sitting in a vehicle or is already knocked down. (server only)
	function Ragdoll(self, seconds: number?, velocity: Vector3?): boolean
	--- Stand a knocked-down character back up now, wherever it lies; Recovered fires when it is up. Safe to call at any time. Returns false when the character was not knocked down. (server only)
	function Recover(self): boolean
	--- Give the player the crouch key back after ForceCrouch. Returns false when the player has no character on foot. (server only)
	function ReleaseCrouch(self): boolean
	--- Give the player a fresh character with full health right now, alive or dead: at position when given, otherwise at the spawn point the place's respawn mode picks. The old character (or a corpse waiting for its respawn) is removed. Returns false when the player is driving a vehicle (eject them first) or cannot be spawned. (server only)
	function Respawn(self, position: Vector3?): boolean
	--- Hurt the character through the same path weapons use, so the game's damage rules apply: friendly fire, Armor, then Shield, then health. source is the player to credit: if this hit kills, Died names them as the killer. damageType tags the hit for Damaged and Died ("Generic" when left out); use a built-in such as "Bullet", "Melee", "Explosion", "Fall", "Vehicle" or "Area", or any word of your own up to 32 characters, such as "Poison". Returns false when the player has no character on foot (respawning or driving), is already dead, or nothing got through (friendly fire off, or Armor 100). (server only)
	function TakeDamage(self, amount: number, source: Player?, damageType: string?): boolean
	--- Move the player's character instantly. Always lands exactly at position, even inside solid geometry; returns false only when the player has no character to move right now. (server only)
	function Teleport(self, position: Vector3): boolean
end

--- Returned by Signal:Connect; call Disconnect to stop listening.
declare extern type Connection with
	--- False once disconnected.
	Connected: boolean
	--- Stop calling the handler.
	function Disconnect(self): ()
end

--- A named network event between server and clients.
declare extern type Remote with
	--- The remote's name.
	Name: string
	--- The server fired this remote to this player (client only).
	OnClientEvent: Signal<(...any)>
	--- A client fired this remote (server only).
	OnServerEvent: Signal<(Player, ...any)>
	--- Send arguments to every player's OnClientEvent handlers. (server only)
	function FireAllClients(self, ...: any): ()
	--- Send arguments to one player's OnClientEvent handlers. (server only)
	function FireClient(self, player: Player, ...: any): ()
	--- Send arguments to the server's OnServerEvent handlers. (client only)
	function FireServer(self, ...: any): ()
end

--- One joint between physics entities, from Constraints:Create or Constraints:FromEntity (server only).
declare extern type Constraint with
	--- The joint broke: something pulled it harder than its BreakForce. Fires once, on the server; the entities move freely from then on.
	Broken: Signal<(Constraint)>
	--- Remove the joint for good: the entities move freely again, and Broken never fires. A joint a script made also goes when every script restarts. (server only)
	function Destroy(self): ()
end

--- One CrowdSpawner's ambient crowd, from Crowds:Get (server only).
declare extern type Crowd with
	--- How much of the spawner's crowd walks, from 0 (nobody) to 1 (the full count the place set, the default). 0.5 of a crowd of 40 is 20 people. (server only)
	Density: number
	--- Whether the crowd walks at all. Starts as the spawner's own enabled setting; false removes every one of its people. (server only)
	Enabled: boolean
	--- True freezes every one of the crowd's people where they stand, the same moment for everyone; false lets them walk on from there. (server only)
	Paused: boolean
	--- The whole number every player's crowd is made from, 0 to 2147483647. Starts as the spawner's own seed; another seed is another crowd: as many people, on the same lanes, but other people in other places. (server only)
	Seed: number
end

--- A character NPCs:Spawn made. The server walks and animates it; every player sees it.
declare extern type NPC with
	--- False once the NPC has been destroyed (Destroy, Entity:Destroy, or it fell out of the world). (server only)
	Exists: boolean
	--- The NPC's health, from 0 to MaxHealth. Weapons, TakeDamage and Heal change this same value. Setting it clamps to 0..MaxHealth, and 0 kills the NPC with no killer. Setting it on a dead NPC does nothing: use Respawn to bring it back. (server only)
	Health: number
	--- The most health the NPC can have (100 unless a script changes it). Setting it clamps to 1..10000; lowering it below Health lowers Health too, and raising it does not heal. Respawn refills health to this maximum. (server only)
	MaxHealth: number
	--- Where the NPC stands: the middle of its body, in centimetres, as Player.Position reads a character. Read-only: use MoveTo to walk it. (server only)
	Position: Vector3
	--- Which way the NPC faces, in degrees as (pitch, yaw, roll). It turns to face the way it walks. Read-only. (server only)
	Rotation: Vector3
	--- How fast the NPC walks, in centimetres per second (150 unless a script changes it; a player runs at 500). Clamped to 0..1500; 0 stops it where it is. The walk clip speeds up and slows down with it. (server only)
	WalkSpeed: number
	--- The NPC died: its health reached 0. killer is the player whose damage finished it (a weapon, or TakeDamage with a source); nil for Kill, Health set to 0, or damage nobody is credited with. A dead NPC stops, falls limp (or plays the clip a Died handler starts with PlayClip or PlayAssetClip, holding its last frame) and stays where it fell, still counting toward the 50, until Respawn or Destroy. Fires on the server only.
	Died: Signal<(NPC, Player?)>
	--- A drive DriveTo or DriveAlong started ended: result is "arrived" when the car stopped within ArriveRadius of the last point, "stuck" when it could not get further (it reversed and tried again, then gave up), or "cancelled" (StopDriving, a new drive, ExitVehicle, the NPC dying or being destroyed, or the car going away). Fires on the server only, once per drive.
	DriveFinished: Signal<(string)>
	--- The NPC's health changed: damage, healing, Health or MaxHealth set, or a respawn's refill. Fires on the server only.
	HealthChanged: Signal<(NPC, number, number)>
	--- A MoveTo walk ended: success is true when the NPC arrived, false when it could not get there. Fires on the server only, once per walk; a walk replaced by another MoveTo, or ended by Stop or Destroy, never fires.
	MoveCompleted: Signal<(boolean)>
	--- Remove the NPC for everyone. Its walk ends without MoveCompleted, and it no longer counts toward the 50. (server only)
	function Destroy(self): ()
	--- As DriveTo, through points in order (one to 64 positions): the car drives each leg in turn, passing the points on the way without stopping, and ends at the last, which is where it arrives. ArriveRadius is the last point's radius. (server only)
	function DriveAlong(self, vehicle: Entity, points: {Vector3}, options: {MaxSpeed: number?, ArriveRadius: number?}?): ()
	--- Put the NPC in the driver's seat of vehicle and drive it to position: the NPC sits in the car for every player, which drives straight at the point, and DriveFinished fires when the drive ends. The NPC must be alive and within 4.5 m of the car with a clear line to the seat; it is the car's driver, so no player gets the Drive prompt or can take the seat, and the car errors if it already has a driver or the NPC is driving another car (StopDriving or ExitVehicle first). It replaces a drive the NPC is on in the same car (that one finishes as "cancelled"). options: MaxSpeed (centimetres per second, 1400 by default, about 50 km/h; clamped to 100..3000) and ArriveRadius (centimetres, 600 by default; clamped to 100..5000). The NPC drives straight at the point over open ground: there are no roads to follow, and it does not steer round walls (a car that can't get on reverses and tries again, then gives up as "stuck"). Cars and trucks only, not motorcycles or aircraft. Arrived, the NPC stays in the seat with the car braked until ExitVehicle. (server only)
	function DriveTo(self, vehicle: Entity, position: Vector3, options: {MaxSpeed: number?, ArriveRadius: number?}?): ()
	--- Get the NPC out of the car it is driving, standing beside it, and free the seat. A drive in progress ends as "cancelled". Returns false when the NPC is not in a car, the car is still moving faster than a walk (3 m/s; StopDriving, then try again once it has stopped) or there is nowhere to stand beside it. (server only)
	function ExitVehicle(self): boolean
	--- The NPC as an Entity: what a raycast that hits it reports. Tag it or give it attributes like any entity. (server only)
	function GetEntity(self): Entity
	--- Raise the NPC's health by amount, up to MaxHealth. Never brings a dead NPC back (use Respawn). Returns false when the NPC is dead or already at full health. (server only)
	function Heal(self, amount: number): boolean
	--- Kill the NPC outright: Died fires with no killer. Returns false when it is already dead. (server only)
	function Kill(self): boolean
	--- Walk to position along the ground, around walls and obstacles, replacing any walk in progress. Returns false at once when position is more than 500 m away or the NPC is dead; otherwise true, and MoveCompleted fires when the walk ends: true when the NPC arrived, false when there is no way there (no walkable ground, a blocked or too long path). The first walk in a place waits for its navigation to build, which can take a few seconds. (server only)
	function MoveTo(self, position: Vector3): boolean
	--- Play one of the place's AnimClip assets on the NPC, starting now, on every player's screen and in step, players who join later included. params: Loop (false by default: the clip plays once and the NPC goes back to walking and standing on its own; true keeps it playing until StopClip or the next PlayClip), PlayRate (clip seconds per second, 1 by default, clamped to 0..10) and StartOffset (seconds into the clip to start at, 0 by default, clamped to 0..3600). The clip must be authored on the skeleton of the NPC's body (the avatars' MetaHuman body). Errors when the id is not an AnimClip in the place's assets or, where the server draws the NPC, the clip is on another skeleton; a dedicated server never loads clips, so it cannot check the skeleton: there a clip on another skeleton shows on each player's screen as the NPC walking and standing on its own instead, with no error for the script. (server only)
	function PlayAssetClip(self, clipAssetId: string, params: {Loop: boolean?, PlayRate: number?, StartOffset: number?}?): ()
	--- Play one of the NPC clips on every player's screen: "walk", "idle", "turn_left" or "turn_right", or (script API 1.23) one of the place's AnimClip asset ids at normal speed (PlayAssetClip also sets its speed and start). It plays once and the NPC goes back to walking and standing on its own, or with loop true keeps playing until StopClip or the next PlayClip. Players who join later see it too. A clip on another skeleton than the NPC's body can't be checked on a dedicated server, which never loads clips; each player's screen then shows the NPC walking and standing on its own instead. (server only)
	function PlayClip(self, clip: string, loop: boolean?): ()
	--- Bring the NPC back with full health, dead or alive: standing at position (its feet) when given, otherwise where NPCs:Spawn first put it. Its clip stops, and a walk in progress ends without MoveCompleted. Returns false when it could not be put there. (server only)
	function Respawn(self, position: Vector3?): boolean
	--- Stop walking where the NPC is. The walk it abandons never fires MoveCompleted. (server only)
	function Stop(self): ()
	--- End the clip PlayClip started: the NPC walks and stands on its own again. (server only)
	function StopClip(self): ()
	--- End the drive the NPC is on: the car brakes to a stop with the NPC still in the seat, and DriveFinished fires with "cancelled". Does nothing when the NPC is not driving. (server only)
	function StopDriving(self): ()
	--- Hurt the NPC through the same path weapons use. source is the player to credit: if this hit kills, Died names them as the killer. Returns false when the NPC is already dead or the damage was refused. (server only)
	function TakeDamage(self, amount: number, source: Player?): boolean
end

--- A named store of whole-number values for this place, read back sorted: a leaderboard. Get one with DataStore:GetOrderedDataStore (server only). Every method returns a Signal that fires once with a result table {ok, error?, ...}: call :Wait() on it straight away, or :Once(handler).
declare extern type OrderedDataStore with
	--- Read key's value: the result's value, nil when the store has no such key. (server only)
	function GetAsync(self, key: string): Signal
	--- One page of the store in value order, highest first (lowest first when ascending): the result's entries is a list of {key, value}, up to pageSize (default 50, at most 100), ties in key order. Pass the result's nextCursor back, with the same ascending, for the next page; it is nil on the last one. (server only)
	function GetSortedAsync(self, ascending: boolean, pageSize: number?, cursor: string?): Signal
	--- Add delta (default 1; may be negative) to key's value, a new key starting from 0. One step on the backend, so no server's increment is lost to another's. The result's value is the new total. (server only)
	function IncrementAsync(self, key: string, delta: number?): Signal
	--- Delete key from the store. Removing a key the store doesn't have succeeds. (server only)
	function RemoveAsync(self, key: string): Signal
	--- Store a whole number (up to 2^53 - 1 either way) under key. The result's value is what was stored. (server only)
	function SetAsync(self, key: string, value: number): Signal
end

--- One sound Sounds:Play started. A server script's handle reaches every player who hears the sound.
declare extern type SoundHandle with
	--- Whether the sound is still playing (or still downloading, to play when it arrives). False once it is stopped, or a one-shot has finished. A dedicated server never downloads sounds, so there a one-shot counts as playing for up to a minute unless it is stopped.
	function IsPlaying(self): boolean
	--- Change the sound's volume, 0 to 2. From a server script, players hear the change within a tenth of a second.
	function SetVolume(self, volume: number): ()
	--- Stop the sound, fading it out over fadeOut seconds (0, the default, stops it at once). Stopping a sound that has already finished does nothing.
	function Stop(self, fadeOut: number?): ()
end

--- A UI document from the place's UI/ folder, open on the local player's screen (UI:OpenDocument). It belongs to the script that opened it and closes when that script stops.
declare extern type UiDocument with
	--- True until the document closes: your Close, Back on a modal document, a budget it went over, or its script stopping. (client only)
	IsOpen: boolean
	--- The document closed by itself: reason is "back" (the player pressed Back on a modal document), "load failed" (its markup or styles could not load), or the budget it went over ("element budget" or "layout time"). Never fires for your own Close, or when your script stops or the place unloads.
	Closed: Signal<(string)>
	--- The document called emit(name, ...) from a data-event-* attribute; args holds the values it passed. Example: shop.Event:Connect(function(name, args) if name == "buy" then buy(args[1]) end end)
	Event: Signal<(string, {any})>
	--- Close the document. Closed does not fire for your own Close, and closing twice is fine. Example: shop:Close() (client only)
	function Close(self): ()
	--- Set a value the document shows through {{key}}, data-for, data-if and data-attr-*: a number, string, boolean, Vector3 (read as .x, .y and .z), or a table of them. An entity, a player or any other object reference is refused: send its fields instead. The key is a name of letters, digits and _ (up to 64). After the document closes it still checks the key and value, then does nothing. Example: shop:SetData("coins", 120) (client only)
	function SetData(self, key: string, value: any): ()
	--- Switch how the document takes input: "modal", "overlay" or "hud" (see UI:OpenDocument). Does nothing once it has closed. Example: shop:SetMode("hud") (client only)
	function SetMode(self, mode: string): ()
end

--- One node in a script's retained UI tree, mounted into the local player's HUD.
declare extern type UiNode with
	--- This node's id, stable for its lifetime.
	Id: number
	--- This node's current text, after any SetLocalizedText resolution.
	Text: string
	--- This node was activated: a mouse click, Enter, Space, or a focused gamepad Accept on a button, toggle or prompt. Only reaches handlers connected by the script that created this node -- nothing else ever holds its handle.
	Activated: Signal<(number, string)>
	--- Make this top-level node float over an entity, a player or an NPC in the world, on this player's screen only: a nameplate, a health bar, a marker. Build the node and its children as usual, then attach the top one; it stops being part of the screen's layout and follows the target, bottom-centre on the point above it. It is only drawn, never clicked. Attaching it again changes the options. Returns false if the node is not a top-level node of this script (call SetParent(nil) first) or the script already has 32 nodes floating; the nodes still count toward the node budget (256). If the target is destroyed, or a player has no character, the node hides. options: Bone (a bone or socket of the target's skeletal mesh, such as "head"; without it, or when the target has no such bone, the node sits over the top of the target), Offset (a Vector3 in centimetres, in world axes, added to that point, such as Vector3.new(0, 0, 30)), ScreenOffsetX and ScreenOffsetY (pixels, x right and y down), MaxDistance (centimetres from the camera, default 3000, at most 50000: farther is hidden), FadeDistance (centimetres: it fades out over the last FadeDistance before MaxDistance; default 0 cuts off at MaxDistance), HideWhenOccluded (hide while world geometry is between the camera and the point; one cheap check a tenth of a second; default false: it draws over everything), HideForSelf (hide when the target is the local player's own character, for a nameplate you do not want over yourself), ScaleWithDistance (default false: constant size on screen; true makes it smaller with distance), MinScale and MaxScale (0.05 to 4, defaults 0.4 and 1; the limits when it scales), ScaleDistance (centimetres where it draws at scale 1, default 1000), OffScreen ("Hide", the default, or "Clamp" to keep it on the edge of the screen pointing towards the target, for a marker). Example: nameplate:AttachToWorld(player, {Bone = "head", Offset = Vector3.new(0, 0, 30), MaxDistance = 2500, FadeDistance = 500, HideForSelf = true}) (client only)
	function AttachToWorld(self, target: Entity | Player | NPC, options: {Bone: string?, Offset: Vector3?, ScreenOffsetX: number?, ScreenOffsetY: number?, MaxDistance: number?, FadeDistance: number?, HideWhenOccluded: boolean?, HideForSelf: boolean?, ScaleWithDistance: boolean?, MinScale: number?, MaxScale: number?, ScaleDistance: number?, OffScreen: string?}?): boolean
	--- Remove this node and every descendant. (client only)
	function Destroy(self): ()
	--- Stop floating this node: it goes back to the screen's layout, as a top-level node. Returns false if it was not attached. Destroy the node instead to remove it altogether. (client only)
	function DetachFromWorld(self): boolean
	--- Name the action this node fires on Activated when clicked, tapped or confirmed. (client only)
	function SetAction(self, action: string): boolean
	--- Attach caption text shown underneath this node when the player has captions on. (client only)
	function SetCaption(self, caption: string): boolean
	--- Enable or disable this node (a disabled button/toggle/prompt cannot be activated). (client only)
	function SetEnabled(self, enabled: boolean): boolean
	--- Set an image node's asset (an "ast_" id from the place's asset library). (client only)
	function SetImage(self, assetId: string): boolean
	--- Look up key in the place's localisation table for the player's locale (falling back through the language, then the place's default locale, then fallback or key itself) and set it as this node's text. (client only)
	function SetLocalizedText(self, key: string, fallback: string?): boolean
	--- Reparent this node (nil detaches it back to the root). Refuses a cycle or a node from another script. (client only)
	function SetParent(self, parent: UiNode?): boolean
	--- Describe this node for screen readers and controller focus order: "none", "heading", "label", "button", "checkbox", "image", "list-container", "list-item", "dialog" or "status". (client only)
	function SetSemantics(self, role: string, label: string?, hint: string?, focusOrder: number?): boolean
	--- Replace this node's style: width, height, columns, font-size; grow (0-100, share of the parent's spare height), gap (0-256, space between children), padding (0-256, space inside the node), opacity (0-1), align ("start", "center", "end" or "stretch", within the parent). Returns false for an unknown key or an out-of-range value; a non-finite number is an error. (client only)
	function SetStyle(self, style: {[string]: boolean | number | string}?): boolean
	--- Set this node's literal text. (client only)
	function SetText(self, text: string): boolean
	--- Show or hide this node (and its subtree). (client only)
	function SetVisible(self, visible: boolean): boolean
end

--- Spawn the place's own meshes at runtime (server only). Change any mesh's look through its Entity.
declare extern type AssetsService with
	--- Spawn one of the place's StaticMesh assets (any mesh its asset list names, placed or not) that every player sees, and return it as an Entity. props sets how it starts: Rotation, Scale (a Vector3, or one number for all three), Material (one of the place's Material assets, for every slot), Color, Transparency, Collision ("Blocking" by default, "Overlap" or "None"), CastShadow (true by default) and Anchored (spawned meshes stay where scripts put them; false is refused). A mesh uploaded with the place may take a moment to appear on a player's screen while it downloads. Spawned meshes count towards the same limit as Entity:Clone; Destroy one to make room. (server only)
	function Spawn(self, assetId: string, position: Vector3, props: {Rotation: Vector3?, Scale: Vector3?, Material: string?, Color: string?, Transparency: number?, Collision: string?, CastShadow: boolean?, Anchored: boolean?}?): Entity
end
declare Assets: AssetsService

--- The local player's avatar: open the avatar screen and play their equipped emotes. On the server, read a player's look with Player.Avatar and Player.Wearing.
declare extern type AvatarService with
	--- Open Mantle's avatar screen, where the player changes their own look. Returns false when there is no local player to open it for. A script cannot change the look itself. (client only)
	function OpenCustomizer(self): boolean
	--- Play one of the local player's equipped emotes, by its item id (an emote on their emote wheel). Returns false, and plays nothing, when itemId is not one of their equipped emotes, when they have no character, or when called again too soon. true means the request went to the server, which checks it again and can still refuse it (the player is moving, in the air, seated or dead). (client only)
	function PlayEmote(self, itemId: string): boolean
end
declare Avatar: AvatarService

--- Give players the built-in Tools and weapons, take them away, and equip them (server only).
declare extern type BackpackService with
	--- Empty the player's Backpack, leaving only fists. Returns how many Tools were removed. (server only)
	function Clear(self, player: Player): number
	--- Put the Tool in this Backpack slot in the player's hands. Slots count from 0, so the hotbar key 1 is slot 0. Returns false when the slot is empty or the player is dead. (server only)
	function EquipSlot(self, player: Player, slot: number): boolean
	--- The ids GrantBuiltIn accepts in this build: "grenade", "healthpack" and "spawnpad". "fists" is never listed. No weapon is built in (editor test sessions also list their test guns). (server only)
	function GetBuiltInTools(self): {string}
	--- The ids of the Tools in the player's Backpack, in slot order. Fists, which every player always has, is never listed. Empty while the player has no character (respawning). (server only)
	function GetTools(self, player: Player): {string}
	--- Give the player a weapon this place stores: item is the library item id Storage:Get returns for one of the place's storage names. Returns the new Tool's stable id. Errors for anything else -- an asset id, a built-in tool's name, an object path, an item the place does not store, or a vehicle. Also errors if the stored weapon has not finished preparing or the player cannot receive it. (server only)
	function Grant(self, player: Player, item: string): string
	--- Give the player one of the built-in Tools: "grenade", "healthpack" or "spawnpad". Returns the new Tool's id, or nil when the player has no character right now, already has that Tool, or has a full Backpack. A player holding nothing equips it at once. "fists" is accepted and does nothing: every player always has fists. No weapon is built in: give one from the place's storage with Grant. (Editor test sessions also take the test guns GetBuiltInTools lists there.) (server only)
	function GrantBuiltIn(self, player: Player, tool: string): string?
	--- Take the Tool with this id out of the player's Backpack (unequipping it first). Returns false when the player does not have it. Fists can never be removed. (server only)
	function Remove(self, player: Player, toolId: string): boolean
	--- Paint one of the player's weapons: color is "#RRGGBB", the same format as Vehicles:SetPaintColor. Every player sees the new colour. Returns false when the player does not have that Tool or it is not a weapon. (server only)
	function SetWeaponColor(self, player: Player, toolId: string, color: string): boolean
end
declare Backpack: BackpackService

--- The local player's view (client only): first person, a fixed camera (cut to or tweened into), a camera following an entity, the field of view and a shake. All of it goes back to Mantle's own camera when the script stops or the player respawns.
declare extern type CameraService with
	--- How wide the view is, in degrees, clamped to 30 to 120, or nil for Mantle's own (90). Aiming down sights still zooms in from it. (client only)
	FieldOfView: number?
	--- "Default" (Mantle's own camera, which the player can switch between first and third person), "FirstPerson" (held in first person: the player cannot switch out), "Fixed" (set by SetFixed or TweenFixed) or "Follow" (set by Follow). Set it to "Default" or "FirstPerson"; setting "Default" also ends a fixed or following camera. First person applies while on foot. (client only)
	Mode: string
	--- Keep the camera at offset from entity, looking at it, wherever it goes (Mode becomes "Follow"). offset is in world axes, so Vector3.new(0, -600, 300) keeps the camera 6 m along -Y and 3 m up; with relative true it turns with the entity's heading instead, so Vector3.new(-600, 0, 250) stays behind it. The player still moves their character. When the entity is destroyed the camera goes back to Mantle's; set Mode to "Default" or call Reset to give it back sooner. (client only)
	function Follow(self, entity: Entity, offset: Vector3, relative: boolean?): ()
	--- Give the camera back to Mantle: Mode "Default", Mantle's field of view, no shake. (client only)
	function Reset(self): ()
	--- Hold the camera still at position, looking at lookAt (Mode becomes "Fixed"). The player still moves their character. Set Mode to "Default" or call Reset to give the camera back. (client only)
	function SetFixed(self, position: Vector3, lookAt: Vector3): ()
	--- Shake the view, fading out over duration seconds. intensity is clamped to 0 to 1 and duration to 0 to 3; a new shake replaces one still running, and 0 stops it. Does nothing when the player turned on Reduced motion. (client only)
	function Shake(self, intensity: number, duration: number): ()
	--- Move the camera smoothly from where the view is now to position, looking at lookAt, over seconds (0 to 60), then hold it there like SetFixed. Mode is "Fixed" from the start. style and direction ease it as they do Entity:Tween (Linear and Out unless given). Returns a signal that fires once when the camera arrives; it never fires if SetFixed, TweenFixed, Follow, Mode or Reset takes the camera first. (client only)
	function TweenFixed(self, position: Vector3, lookAt: Vector3, seconds: number, style: string?, direction: string?): Signal
end
declare Camera: CameraService

--- React to what players say in chat, and post lines from the place.
declare extern type ChatService with
	--- A player said something in chat. text is what everyone saw: already filtered, so masked words stay masked. On the server it fires for every player's line, a second or so after it was sent; on a client, for each player line this player's chat box shows (so never for someone they blocked). Lines from Chat:SendSystemMessage never fire it.
	MessagePosted: Signal<(Player, string)>
	--- Post a line from the place into chat, drawn apart from what players say: to everyone, or only to player when you pass one. It goes through the same chat filter as player text, so a line with words that aren't allowed is masked or never shown. At most 500 characters, and at most 10 lines a minute per server. Returns false when it cannot be sent right now: over that limit, the player has no verified account yet, or this server has no session chat (a local test run). (server only)
	function SendSystemMessage(self, text: string, player: Player?): boolean
end
declare Chat: ChatService

--- Join physics entities with welds, hinges and ropes while the place runs (server only).
declare extern type ConstraintsService with
	--- Join two physics entities, or one to the world, and return the joint. Type is "weld" (holds them as they are), "hinge" (EntityA turns about Axis, and only that) or "rope" (the anchors stay within Length of each other). EntityA is the physics entity the joint holds and EntityB another; leave EntityB out to join EntityA to the world. AnchorA is where on EntityA the joint is, in its own space ((0, 0, 0), its middle, by default); AnchorB where on EntityB (a point in the world without it), by default wherever AnchorA is now. A hinge's Axis is a direction in EntityA's own space ((0, 0, 1) by default) and Limits is {min, max}, the degrees it may turn from where it is now, min at most 0 and max at least 0. A rope's Length is in cm, by default how far apart the anchors are now. Pull the joint harder than BreakForce newtons and it breaks for good (Broken fires). The two entities stop colliding with each other. Players see the entities move; nothing of the joint itself is sent. A place holds at most 200 joints, its own included. (server only)
	function Create(self, params: {Type: string, EntityA: Entity, EntityB: Entity?, AnchorA: Vector3?, AnchorB: Vector3?, Axis: Vector3?, Limits: {number}?, Length: number?, BreakForce: number?}): Constraint
	--- The joint a place's Constraint entity made, as a Constraint, so a script can hear it break or remove it; nil for any other entity. (server only)
	function FromEntity(self, entity: Entity): Constraint?
end
declare Constraints: ConstraintsService

--- Control the ambient crowds a place's CrowdSpawners walk: how many, on or off, moving or frozen (server only). Hear where their people go (client only).
declare extern type CrowdsService with
	--- One of a CrowdSpawner's people arrived at one of its points of interest and stops there a while (client only). npc says which of the crowd's people it was: the same number is the same person on every player's screen, and they all see the arrival at about the same moment. Play a greeting, open a shop's door, count visitors. Arrivals during a clock re-sync right after joining are not reported.
	PointReached: Signal<(Entity, Entity, number)>
	--- The crowd of a CrowdSpawner the place placed. Every player sees what you set on it. Errors if the entity is not a CrowdSpawner (a clone of one has no crowd) or no longer exists. (server only)
	function Get(self, spawner: Entity): Crowd
end
declare Crowds: CrowdsService

--- Durable saved data for this place (server only).
declare extern type DataStoreService with
	--- This place's saved value for key, from the local cache (nil if never set, removed, or not loaded yet). (server only)
	function Get(self, key: string): any
	--- This player's saved value for key, from the local cache. (server only)
	function GetForPlayer(self, player: Player, key: string): any
	--- The ordered store with this name: whole-number values for this place, read back sorted, for leaderboards. A place can fill 32 stores. (server only)
	function GetOrderedDataStore(self, name: string): OrderedDataStore
	--- One page (up to 100) of this place's saved keys that start with prefix, in key order, from the local cache like Get. Pass nextCursor back for the next page; it is nil on the last one. (server only)
	function ListKeys(self, prefix: string?, cursor: string?): {keys: {string}, nextCursor: string?}
	--- One page of this player's saved keys, like ListKeys. (server only)
	function ListKeysForPlayer(self, player: Player, prefix: string?, cursor: string?): {keys: {string}, nextCursor: string?}
	--- Delete key from this place's saved data. Get reads nil at once; removed from the backend asynchronously, like Set. (server only)
	function RemoveKey(self, key: string): ()
	--- Delete key from this player's saved data, like RemoveKey. (server only)
	function RemoveKeyForPlayer(self, player: Player, key: string): ()
	--- Save key for this place: a boolean, number, string, or table of those (nested up to 4 deep, 64 KB encoded). Applies to the local cache immediately; saved to the backend asynchronously. (server only)
	function Set(self, key: string, value: any): ()
	--- Save key scoped to this player, with the same values as Set. Applies to the local cache immediately; saved to the backend asynchronously. (server only)
	function SetForPlayer(self, player: Player, key: string, value: any): ()
end
declare DataStore: DataStoreService

--- Show Mantle's built-in particle effects anywhere in the world: smoke, fire, sparks, dust and glow.
declare extern type EffectsService with
	--- Show a built-in effect at position for a moment: archetype is "Smoke", "Fire", "Sparks", "Dust" or "Glow" (any case). It emits for params.Duration seconds (1 by default, 0 to 10) and is gone once its last particles are. params tune its look, each the archetype's own by default: Color ("#RRGGBB"), Size (particle size, 1 to 1000 cm), Rate (particles a second, 0 to 200), Speed (0 to 5000 cm/s), Lifetime (how long each particle lives, 0.05 to 10 seconds); numbers out of range are clamped. Particles leave along +Z, or along Rotation's up (a Vector3 of pitch, yaw and roll). From a client script only this player sees it; from a server script every player does, but not one who joins later. A dedicated server shows nothing. At most 10 plays a frame per script, and a server script at most 10 a second (bursts of 20); past either the script stops.
	function Play(self, archetype: string, position: Vector3, params: {Color: string?, Size: number?, Rate: number?, Speed: number?, Lifetime: number?, Duration: number?, Rotation: Vector3?}?): ()
end
declare Effects: EffectsService

--- The time of day, the weather, the sky, the fog and the screen's colour, the same for everyone on the server. Everyone can read them; server scripts can change them, and every player sees the change.
declare extern type EnvironmentService with
	--- How strongly bright things glow, from 0 (no glow) to 4, or nil for the game's own. Set it on the server.
	Bloom: number?
	--- The hour, as TimeOfDay. Set it (server) to jump the clock: 21 is night. Any number works and wraps around 24. The clock keeps running from there at TimeScale.
	ClockTime: number
	--- How much of the sky is cloudy right now, from 0 to 1.
	CloudCover: number
	--- A colour-grading look for the whole picture: "Warm", "Cool", "Noir", "Sepia", "Vivid" or "Faded", or nil for none. Set it on the server; ColorGradingIntensity says how strongly it shows.
	ColorGrading: string?
	--- How strongly ColorGrading shows: 1 is the full look, 0 is none of it, from 0 to 1. Set it on the server.
	ColorGradingIntensity: number
	--- Tint the whole picture: "#RRGGBB", where "#FFFFFF" is unchanged. Set it on the server; nil sets it back to "#FFFFFF".
	ColorTint: string
	--- The picture's contrast: 1 is unchanged, from 0 to 2. Set it on the server.
	Contrast: number
	--- How long one day lasts, in real minutes: 30.
	DayLength: number
	--- Brighten (above 0) or darken (below 0) the whole picture, in stops, from -4 to 4. Set it on the server.
	ExposureBias: number
	--- The fog's colour as "#RRGGBB", or nil for the sky's own. Set it on the server.
	FogColor: string?
	--- How thick the fog is, times the weather's own: 1 is unchanged, 0 clears it, up to 4. Set it on the server.
	FogDensity: number
	--- True while the sun is below the horizon.
	IsNight: boolean
	--- How hard it is raining right now, from 0 (dry) to 1 (a storm).
	RainIntensity: number
	--- How colourful the picture is: 1 is unchanged, 0 is black and white, up to 2. Set it on the server.
	Saturation: number
	--- true shows the stars (even by day), false hides them, nil lets them come out at night on their own. Set it on the server.
	StarsVisible: boolean?
	--- How bright the sun (and by night the moon) is, times its own: 1 is unchanged, 0 to 4. Set it on the server.
	SunIntensityScale: number
	--- The hour, from 0 up to 24: 6 is sunrise, 12 is noon, 18 is sunset. Every server starts at 9.
	TimeOfDay: number
	--- How fast the clock runs: 1 is the normal 30-minute day, 0 stops it, 2 is twice as fast. Set it (server) from 0 to 60; the hour carries on from where it is.
	TimeScale: number
	--- How dark the picture's corners are, from 0 to 1, or nil for the game's own. Set it on the server.
	Vignette: number?
	--- The weather, or the weather it is changing to: Clear, PartlyCloudy, Cloudy, Overcast, Foggy, LightRain, Rain, Storm, LightSnow, Snow, Blizzard, DustCalm or DustStorm.
	Weather: string
	--- Lightning flashed (client only), at the same moment for everyone. The default sky already shows the bolt and plays its thunder.
	LightningStrike: Signal<()>
	--- A server script changed an environment property: the argument names it ("ClockTime", "TimeScale", "Weather", "CloudCover", "FogDensity", "FogColor", "StarsVisible", "SunIntensityScale", "ExposureBias", "Saturation", "Contrast", "Vignette", "ColorTint", "Bloom", "ColorGrading" or "ColorGradingIntensity"). Fires everywhere, a player who joins later included. The clock running on by itself is not a change.
	PropertyChanged: Signal<(string)>
	--- The weather started changing. The argument is the new weather, as in Environment.Weather.
	WeatherChanged: Signal<(string)>
	--- Take away this player's own look from SetLocalLook (client only): the picture follows the server's values again. (client only)
	function ClearLocalLook(self): ()
	--- Stop the clock where it is: the same as setting TimeScale to 0. Set TimeScale to 1 to start it again. (server only)
	function FreezeClock(self): ()
	--- Let the weather change on its own again after SetWeather, blending back over blendSeconds (0 to 600, 90 when left out). (server only)
	function ResumeNaturalWeather(self, blendSeconds: number?): ()
	--- Cover this much of the sky with cloud, from 0 to 1, whatever the weather; nil gives the weather its own clouds back. The sun dims and the stars fade under heavy cover. (server only)
	function SetCloudCover(self, cover: number?): ()
	--- Change how the picture looks for this player only (client only). look holds any of ExposureBias (-4 to 4), Saturation (0 to 2), Contrast (0 to 2), Vignette (0 to 1), ColorTint ("#RRGGBB"), Bloom (0 to 4), ColorGrading ("Warm", "Cool", "Noir", "Sepia", "Vivid" or "Faded") and ColorGradingIntensity (0 to 1), taken as the properties of the same names take them. What look sets wins over the server's values; what it leaves out follows them. Each call replaces the last look whole, and the look goes when this script stops. Time and weather stay the same for everyone. From the server, send one player a look with a Remote's FireClient and call this in its OnClientEvent. (client only)
	function SetLocalLook(self, look: {ExposureBias: number?, Saturation: number?, Contrast: number?, Vignette: number?, ColorTint: string?, Bloom: number?, ColorGrading: string?, ColorGradingIntensity: number?}): ()
	--- Hold the weather at Clear, PartlyCloudy, Cloudy, Overcast, Foggy, LightRain, Rain, Storm, LightSnow, Snow, Blizzard, DustCalm or DustStorm until ResumeNaturalWeather, changing to it over blendSeconds (0 to 600, 90 when left out). A Storm brings lightning. The snow and dust weathers (LightSnow, Snow, Blizzard, DustCalm, DustStorm) never come on their own. (server only)
	function SetWeather(self, weather: string, blendSeconds: number?): ()
end
declare Environment: EnvironmentService

--- Named events that let scripts on the same side talk to each other, in any language.
declare extern type EventsService with
	--- The event with this name; connect to it or Fire it. Doesn't cross the network (use Net:Remote for that).
	function Get(self, name: string): Signal
end
declare Events: EventsService

--- The local player's keys, mouse buttons and gamepad buttons, and their sticks, triggers and mouse wheel as axes (client only). Mantle's own controls (the menu, chat, voice, movement, the Backpack and Tool keys) can never be bound or read.
declare extern type InputService with
	--- Call callback when the player presses one of keys (phase "Began") and when they let go (phase "Ended"); key is the name of the key that did it. keys lists 1 to 4 names such as {"Q", "ButtonY"}: letters, "0" to "9", "F1" to "F10", "Tab", "MouseLeft", "MouseRight", "MouseMiddle", "MouseButton4", "MouseButton5", "ButtonA", "ButtonB", "ButtonX", "ButtonY", "ButtonL1" and "ButtonR1". A key Mantle uses for its own controls is an error; GetBindableKeys lists the free ones. Binding a name again replaces that action. The key still does anything Mantle does with it (F and ButtonY also enter and leave cars). Nothing is heard while Mantle's menu or chat is open. (client only)
	function BindAction(self, name: string, keys: {string}, callback: (phase: string, key: string) -> ()): ()
	--- Call callback when one of axes moves, at most once a frame per axis; axis is the name of the one that moved. axes lists 1 to 4 of "LeftStickX", "LeftStickY", "RightStickX", "RightStickY" (-1 to 1, right and up positive, 0 inside the stick's dead zone), "LeftTrigger", "RightTrigger" (0 to 1) and "MouseWheel" (the notches scrolled that frame, up positive; heard only on frames it moves). A stick or trigger whose movement is one of Mantle's own controls is an error; GetBindableAxes lists the free ones. A trigger Mantle uses as a button (fire, aim) can still be read here: the button keeps working, and no script can bind it as a key. Binding a name again replaces that binding; axis bindings and BindAction's actions have separate names. The control still does anything Mantle does with it (the mouse wheel also steps Mantle's camera in and out). Everything reads 0 while Mantle's menu or chat is open. (client only)
	function BindAxis(self, name: string, axes: {string}, callback: (value: number, axis: string) -> ()): ()
	--- Where axis is right now, by the names BindAxis takes, as its callbacks last heard it: "MouseWheel" reads the notches of the last frame. 0 while Mantle's menu or chat is open. An axis that is part of Mantle's own controls is an error. (client only)
	function GetAxis(self, axis: string): number
	--- Every axis name this place can bind: the names BindAxis takes, minus the sticks and triggers whose movement Mantle uses for its own controls. A trigger Mantle uses only as a button (fire, aim) is listed. (client only)
	function GetBindableAxes(self): {string}
	--- Every key name this place can bind: the names BindAction takes, minus Mantle's own controls. (client only)
	function GetBindableKeys(self): {string}
	--- Whether the player is holding key down right now, by the same names BindAction takes. Always false while Mantle's menu or chat is open. A key Mantle uses for its own controls is an error. (client only)
	function IsKeyDown(self, key: string): boolean
	--- Remove the action this script bound under name. Returns false when it has none by that name. A script's actions also go away by themselves when it stops. (client only)
	function UnbindAction(self, name: string): boolean
	--- Remove the axis binding this script made under name. Returns false when it has none by that name. A script's axis bindings also go away by themselves when it stops. (client only)
	function UnbindAxis(self, name: string): boolean
end
declare Input: InputService

--- Let players walk up to an object and act on it with a key press, and hear when they do. A place can mark an object interactable itself; here a script makes one, changes what players can do to it, and reacts. The server decides everything: change things and listen to Interacted on the server. The client events are for the local player and only for effects such as a sound or a glow.
declare extern type InteractionService with
	--- The local player let go of a hold or mash, or its progress drained away, or the part stopped being usable (client only; for effects). A hold or mash that finishes fires Interacted on the server instead.
	Cancelled: Signal<(Entity, string, string)>
	--- A player used one of the entity's options (server only). part is the id of the part they used ("" for the object as a whole) and action is "Primary" or "Secondary". A hold or mash fires it once, when it is finished. This is the event that counts: it is checked on the server, so a player out of range, behind a wall or on cooldown never gets here. Connect to it, check which entity it is, and do what the use should do.
	Interacted: Signal<(Entity, Player, string, string)>
	--- How far the local player's hold or mash has got, 0 to 1, as it changes (client only; for effects such as a rising sound).
	Progress: Signal<(Entity, string, string, number)>
	--- The local player's aim came onto a part of an interactable entity and its prompt appeared (client only; for effects, not for changing the game).
	Selected: Signal<(Entity, string)>
	--- The local player began holding or mashing an option (client only; for effects).
	Started: Signal<(Entity, string, string)>
	--- The local player's aim left a part, or it stopped being usable (client only; for effects).
	Unselected: Signal<(Entity, string)>
	--- Make the entity not interactable any more, for everyone: no prompt, no key. SetOptions makes it interactable again with the options you give. (server only)
	function Remove(self, entity: Entity): ()
	--- Change how the entity's prompt looks, for everyone. Only the fields you give change. Style is "Default" (the key and text of each option, stacked), "Balanced" (a panel with the Title on top and the options under it) or "Informative" (like Balanced, with the Description beside the options and an accent line); Balanced and Informative need a Title and show as Default without one. Title is up to 80 characters, Description up to 200, and AccentColor is "#RRGGBB". (server only)
	function SetDisplay(self, entity: Entity, display: {Style: string?, Title: string?, Description: string?, AccentColor: string?}): ()
	--- Turn one option of the entity off or back on, for everyone. action is "Primary" or "Secondary", the input the option uses. Turning an option on also ends its cooldown early. An option with AfterUse "OneShot" that was used stays used. Pass a part's id for an object with several parts; leave it out for the object as a whole. (server only)
	function SetEnabled(self, entity: Entity, action: string, enabled: boolean, part: string?): ()
	--- Make the entity interactable and set what players can do to it, for everyone. options is a list of at most two tables, one for each input: Key ("Primary" by default, or "Secondary"), Text (what the prompt says beside the key, such as "Open door"; leave it out to show the key alone), HoldSeconds (hold the key this long; 0 is a tap), PressCount (mash the key this many times instead; wins over HoldSeconds), PressDrainPerSecond (how fast a mash's progress drains, 0 to 10), AfterUse ("None", "DisableUntilEnabled", "Cooldown" or "OneShot") and CooldownSeconds (needed above 0 for "Cooldown"). Pass a part's id to change only that part of an object the place gave several parts; leave it out for the object as a whole. An empty list leaves the object with nothing to do. Example: Interaction:SetOptions(door, {{Key = "Primary", Text = "Open door"}}) (server only)
	function SetOptions(self, entity: Entity, options: {{Key: string?, Text: string?, HoldSeconds: number?, PressCount: number?, PressDrainPerSecond: number?, AfterUse: string?, CooldownSeconds: number?}}, part: string?): ()
	--- Set how close a player must stand to the entity, in centimetres, for the prompt to show and the key to work. Clamped to 50..5000. A place's default is 250. (server only)
	function SetRange(self, entity: Entity, range: number): ()
	--- Make one option of the entity unusable for this many seconds (up to 3600), for everyone, as if it had just been used with AfterUse "Cooldown". action is "Primary" or "Secondary". SetEnabled ends it early. Pass a part's id for an object with several parts; leave it out for the object as a whole. (server only)
	function StartCooldown(self, entity: Entity, action: string, seconds: number, part: string?): ()
end
declare Interaction: InteractionService

--- The built-in leaderboard panel: players press Tab (or pick Leaderboard in the game menu) to see the top 10 of one of this place's ordered stores, with each player's name and picture (server only).
declare extern type LeaderboardService with
	--- Take the leaderboard panel down: players no longer see it until Configure shows a board again. The store keeps its entries. (server only)
	function Clear(self): ()
	--- Show the ordered store with this name (as DataStore:GetOrderedDataStore takes it) on the leaderboard panel: its top 10, highest first, or lowest first when options.ascending is true. Key each entry by the player's UserId and the panel shows their name and picture; any other key shows as it is. options.title (up to 32 characters, "Leaderboard" when left out) heads the panel and options.valueLabel (up to 16, "Score" when left out) heads the value column. The panel reads the store now and every 30 seconds while anyone is playing, so it trails the store by up to 30 seconds. Call it again to switch boards; one board shows at a time. (server only)
	function Configure(self, store: string, options: {title: string?, valueLabel: string?, ascending: boolean?}?): ()
end
declare Leaderboard: LeaderboardService

--- Spawn lights at runtime (server only). Change any light through its Entity's Light members.
declare extern type LightsService with
	--- Spawn a light every player sees and return it as an Entity. kind is "Point", "Spot" or "Directional". props sets how it starts: Color ("#RRGGBB", white by default), Intensity (candelas, 8 by default; lux for a directional light, 10 by default), Range (cm, 1000 by default), InnerCone and OuterCone (degrees, 0 and 44 by default), Enabled, CastShadow (off by default: shadows are costly) and Rotation (a spot light points straight down and a directional light down at 45 degrees unless you give one). Numbers out of range are clamped. At most 64 spawned lights exist at once in a place; Destroy one to make room. (server only)
	function Spawn(self, kind: string, position: Vector3, props: {Color: string?, Intensity: number?, Range: number?, InnerCone: number?, OuterCone: number?, Enabled: boolean?, CastShadow: boolean?, Rotation: Vector3?}?): Entity
end
declare Lights: LightsService

--- A small round lifecycle you drive yourself: start a round, end it (by calling EndRound or when its timer runs out), and read the round number and clock. The server runs it; State, Round and TimeRemaining are readable from client scripts too, for a HUD. It does not pick winners, move players or restart anything by itself: connect to RoundStarted and RoundEnded and do what your game needs. The match is Waiting before a round and between rounds, InProgress during one, and Ended after EndMatch.
declare extern type MatchService with
	--- The number of the current round, or of the last one while Waiting: 0 before any round has started, 1 for the first. Reset puts it back to 0.
	Round: number
	--- "Waiting" (before the first round and between rounds), "InProgress" (a round is running) or "Ended" (EndMatch was called; Reset starts over).
	State: string
	--- Seconds left in the running round, counting down to 0, or nil when no round is running or the round has no time limit. On a client this follows the server's clock, so it can be a fraction of a second off.
	TimeRemaining: number?
	--- A round ended, on the server. winner is what EndRound or EndMatch was given (nil for none, and always nil when the time ran out). reason is "Ended" (EndRound), "Time" (the round's timer ran out) or "MatchEnded" (EndMatch or Reset cut it short).
	RoundEnded: Signal<(number, string?, string)>
	--- A round began (StartRound), on the server. round is its number, starting at 1.
	RoundStarted: Signal<(number)>
	--- State changed, on the server. Arguments are the old state, then the new one (each "Waiting", "InProgress" or "Ended"), the same order as PlayerTeamChanged. Fires after RoundStarted or RoundEnded.
	StateChanged: Signal<(string, string)>
	--- End the whole match. A running round ends first (RoundEnded fires with reason "MatchEnded"), then State becomes Ended. Returns false when the match has already ended. (server only)
	function EndMatch(self, winner: string?): boolean
	--- End the running round: RoundEnded fires with winner (a team name, a player's name, anything up to 64 characters, or nil for none) and reason "Ended", and State goes back to Waiting. Returns false when no round is running. (server only)
	function EndRound(self, winner: string?): boolean
	--- Start over: a running round ends first (reason "MatchEnded"), then State is Waiting and Round is 0. Teams and players are not touched. (server only)
	function Reset(self): ()
	--- Begin the next round: Round goes up by one, State becomes InProgress, and RoundStarted fires. With seconds (up to 86400) the round ends by itself when the time is up, with reason "Time" and no winner; without, it runs until EndRound. Returns false, changing nothing, while a round is already running or after EndMatch (call Reset first). (server only)
	function StartRound(self, seconds: number?): boolean
end
declare Match: MatchService

--- Spawn characters the server walks around the place and animates (server only).
declare extern type NPCsService with
	--- The NPC an Entity is (a World:Raycast hit, or NPC:GetEntity()), or nil when it is not a live NPC. (server only)
	function FromEntity(self, entity: Entity): NPC?
	--- Spawn a character standing at position (its feet), facing yaw degrees (0 faces +X), and return it as an NPC. Every player sees it. look.AvatarItemId optionally selects a published avatar body; omit it to spawn an NPC with no rendered body. SkinTone and EyeColor are avatar palette ids ("tone_05"), Height is a percent from 95 to 105. A place holds at most 50 NPCs at once; Destroy one to make room. (server only)
	function Spawn(self, position: Vector3, look: {AvatarItemId: string?, SkinTone: string?, EyeColor: string?, Height: number?}?, yaw: number?): NPC
end
declare NPCs: NPCsService

--- Send messages between the server and players.
declare extern type NetService with
	--- Get the remote with this name (the same name on server and client refers to the same remote).
	function Remote(self, name: string): Remote
end
declare Net: NetService

--- Everyone in the session.
declare extern type PlayersService with
	--- The player on this machine. Always set in client scripts (they start once it exists); nil on a dedicated server.
	LocalPlayer: Player
	--- A player joined after the handler connected. Does not replay existing players; use Players:OnPlayer to handle players already present and future joins.
	PlayerAdded: Signal<(Player)>
	--- A player is leaving.
	PlayerRemoving: Signal<(Player)>
	--- The player whose character this entity is right now, or nil.
	function GetPlayerFromCharacter(self, character: Entity): Player?
	--- Players currently in the session.
	function GetPlayers(self): {Player}
	--- Call handler for every player: those already here and everyone who joins later. The usual way to set players up.
	function OnPlayer(self, handler: (player: Player) -> ()): Connection
end
declare Players: PlayersService

--- How a death becomes a respawn. By default a dead player respawns on their own after a short wait at a spawn point the place picks. Change the wait, or turn automatic respawning off to run rounds where the dead stay down until you bring them back with Player:Respawn. The server decides; there is nothing for a client script here. Player.Respawned fires when a player gets a fresh character.
declare extern type RespawnService with
	--- Whether a dead player respawns by themselves after Delay (true by default). Set it false and the dead stay dead until a script calls Player:Respawn on them. Applies to deaths from then on. (server only)
	AutoRespawn: boolean
	--- Seconds a dead player waits before the game respawns them (5 unless the place or a script changes it). Setting it clamps to 0..600 and applies to deaths from then on, not to a respawn already counting down. (server only)
	Delay: number
end
declare Respawn: RespawnService

--- Facts about the running script.
declare extern type ScriptService with
	--- True when this script runs on a client.
	IsClient: boolean
	--- True when this script runs on the server.
	IsServer: boolean
	--- Display name: the script file's name without extension.
	Name: string
	--- The name this place's storage gives the script package this script belongs to, or "" for one of the place's own scripts. Example: Script.Package is "KillBricks" for Packages/KillBricks/Server/Main.luau.
	Package: string
	--- Path relative to the Scripts folder, e.g. "Server/Rounds.luau".
	Path: string
	--- The Script Points placed for this script's package: each an entity at the point's position, tagged with the package's storage name, its parameters readable with GetAttribute (numbers and true/false already converted, as the package declares them). Points that are switched off are left out. Empty for one of the place's own scripts. A package's scripts run once, not once per point: loop over Script.Points. Example: for _, point in Script.Points do print(point:GetAttribute("speed")) end
	Points: {Entity}
	--- The settings this place gives this script's package, set once for the whole package in Place Settings > Storage: setting name to value, already converted as the package declares them (a number, true/false, or text; a choice is its text). A setting the place leaves unset has the package's default; one with no default and no value is missing (nil). Empty for one of the place's own scripts. A fresh table on every read. Example: local roundLength = Script.Settings.RoundLength
	Settings: {[string]: boolean | number | string}
	--- "server" or "client".
	Side: string
end
declare Script: ScriptService

--- Facts about the server this session runs on (server only).
declare extern type ServerService with
	--- The id of the private server this session belongs to, or nil for a public session. (server only)
	PrivateServerId: string?
	--- The Mantle account id (a plain UUID) of the private server's owner, or nil for a public session. (server only)
	PrivateServerOwnerId: string?
end
declare Server: ServerService

--- Play the sounds uploaded with this place: effects and loops, flat or placed in the world, and one music track per player.
declare extern type SoundsService with
	--- Play one of the sounds uploaded with this place (a sound emitter's Entity.SoundId, or its id in the place's assets). From a client script only this player hears it; from a server script every player does, or only params.Player. Give Position or AttachTo to place it in the world, where it fades to silence at Radius cm (4000 by default); without either it plays flat, like interface sounds. Volume 0 to 2 (1), Pitch 0.5 to 2 (1), Looped (false), FadeIn seconds (0). A sound that has not finished downloading starts when it arrives. Returns a SoundHandle.
	function Play(self, sound: string, params: {Volume: number?, Pitch: number?, Looped: boolean?, Position: Vector3?, AttachTo: Entity?, Radius: number?, FadeIn: number?, Player: Player?}?): SoundHandle
	--- Play one of the place's sounds as music: from a client script this player's, from a server script every player's, or only params.Player's. Each player has one music track: a new one crossfades from the old over FadeIn seconds (1 by default), and playing the track already playing only changes its Volume (0 to 2). Loops unless Looped is false.
	function PlayMusic(self, sound: string, params: {Volume: number?, Looped: boolean?, FadeIn: number?, Player: Player?}?): ()
	--- Stop the music, fading it out over params.FadeOut seconds (0, the default, stops it at once). From a server script: every player's music, or only params.Player's.
	function StopMusic(self, params: {FadeOut: number?, Player: Player?}?): ()
end
declare Sounds: SoundsService

--- Shared game values the server sets and everyone can read (score, round, phase).
declare extern type StateService with
	--- A value changed (fires on the server and on every client).
	Changed: Signal<(string, any)>
	--- The current value, or nil.
	function Get(self, key: string): any
	--- Call handler with the value now and every time it changes. The easy way to keep UI in sync.
	function Observe(self, key: string, handler: (value: any) -> ()): Connection
	--- Set a value for everyone. Numbers, strings, booleans, Vector3 and tables work. (server only)
	function Set(self, key: string, value: any): ()
end
declare State: StateService

--- The library items (vehicles, weapons and script packages) this place stores, by the names its storage gives them.
declare extern type StorageService with
	--- The library item id (itm_u...) of the entry this place's storage declares under name. The name is case-sensitive. Pass the id to Vehicles:Spawn (a stored vehicle) or Backpack:Grant (a stored weapon): they accept only an id the place stores. Errors when the place's storage has no entry with that name.
	function Get(self, name: string): string
end
declare Storage: StorageService

--- Teams for a place: define them, put players on them, and decide whether teammates can hurt each other. The server decides everything; which team a player is on replicates, so client scripts and HUDs can read it. A team is named by its name (not case-sensitive) and has a colour. Friendly fire is off by default: once a player is on a team, damage from a teammate is ignored until Teams.FriendlyFire is set to true.
declare extern type TeamsService with
	--- Whether players on the same team can hurt each other (false by default). When false, damage between teammates is ignored everywhere damage goes through the health system: weapons, melee, explosions and Player:TakeDamage. Hits from nobody, and a player's own hits, still count. Setting it is server only.
	FriendlyFire: boolean
	--- A player joined, left or switched team, on the server and on every client. Arguments are the player, the old team, then the new team (team names; nil means no team).
	PlayerTeamChanged: Signal<(Player, string?, string?)>
	--- Define a team. name is 1 to 32 characters and must be new. color is "#RRGGBB" (white when left out), for a HUD or a nameplate to use. Up to 16 teams. Example: Teams:Create("Red", "#E53935") (server only)
	function Create(self, name: string, color: string?): ()
	--- The team's colour as "#RRGGBB", or nil when there is no such team.
	function GetColor(self, name: string): string?
	--- The players on the team right now (an empty list for a team nobody is on, or one that does not exist).
	function GetPlayers(self, name: string): {Player}
	--- The names of the teams, in the order they were created.
	function List(self): {string}
	--- Delete a team. Its players are taken off it first, so PlayerTeamChanged fires for each. (server only)
	function Remove(self, name: string): ()
end
declare Teams: TeamsService

--- Clocks for scripts.
declare extern type TimeService with
	--- A high-precision timer in seconds, for measuring how long something takes.
	Clock: number
	--- Seconds since the game started, synced to the server's clock on every machine.
	Now: number
end
declare Time: TimeService

--- The local player's on-screen UI: a simple HUD line/toast, and the retained node tree behind real widgets.
declare extern type UIService with
	--- Whether the player wants captions shown.
	Captions: boolean
	--- Whether the player asked for a high-contrast presentation.
	HighContrast: boolean
	--- The player's current locale (e.g. "fr-CA").
	Locale: string
	--- Whether the player asked for reduced motion.
	ReducedMotion: boolean
	--- The player's platform text-scale preference (0.85-1.5).
	TextScale: number
	--- The player's remapped label for an action name (e.g. the bound key), or the action name itself if unset. (client only)
	function ActionLabel(self, action: string): string
	--- Create a new node: "frame", "text", "image", "button", "toggle", "list", "grid", "scroll" or "prompt". Nil means the per-script node budget (256) is exhausted. (client only)
	function Create(self, kind: string): UiNode?
	--- Format a number for display using the player's current locale. (client only)
	function FormatNumber(self, value: number): string
	--- Open one of the place's UI documents (a .rml file in its UI/ folder) on this player's screen. Mode is "modal" (the default: gameplay input stops and Back closes it), "overlay" (play goes on; the mouse works on its controls) or "hud" (drawn only, never takes input). An unknown path or mode is an error, and so is opening on a server. Example: local shop = UI:OpenDocument("UI/shop.rml") (client only)
	function OpenDocument(self, path: string, mode: string?): UiDocument
	--- Force the mouse cursor on or off regardless of whether an interactive node is visible right now. Nil goes back to automatic: shown only while a Button/Toggle/Prompt is visible, enabled and has an action. A gamepad-only panel can call SetCursorVisible(false) to keep working through focus/Accept without ever drawing a cursor. (client only)
	function SetCursorVisible(self, visible: boolean?): ()
	--- Show text in a named HUD line (same slot replaces the previous text; empty text clears it). (client only)
	function SetText(self, slot: string, text: string): ()
	--- Pop up a short message. (client only)
	function Toast(self, text: string, seconds: number?): ()
end
declare UI: UIService

--- Spawn and seat cars: the vehicles in the place's storage (server only).
declare extern type VehiclesService with
	--- Put the player back on foot beside the car they are driving. Returns false when they are not driving, the car is still moving, or there is nowhere beside it to stand. (server only)
	function Eject(self, player: Player): boolean
	--- The car's speed in centimetres per second, as the simulation reports it. Errors if the entity is not a car or no longer exists. (server only)
	function GetSpeed(self, vehicle: Entity): number
	--- Whether the car is locked. A locked car opens only for the last player who drove it: anyone else's vehicle key rattles the door, and holding the key breaks in -- the window on that side breaks, the car unlocks, and it must be hotwired before it starts. The lock stays when the driver gets out, and its driver can also lock and unlock it from the seat (L / D-pad left). Errors if the entity is not a car or no longer exists. (server only)
	function IsLocked(self, vehicle: Entity): boolean
	--- Lock the car. A locked car opens only for the last player who drove it: anyone else's vehicle key rattles the door, and holding the key breaks in -- the window on that side breaks, the car unlocks, and it must be hotwired before it starts. The lock stays when the driver gets out, and its driver can also lock and unlock it from the seat (L / D-pad left). Returns false when the entity is not a car. (server only)
	function Lock(self, vehicle: Entity): boolean
	--- Put the player in the driver's seat, through the same checks as walking up and pressing the prompt: the player must be on foot, alive, near the car with a clear line to the seat, and the seat must be empty. Returns false when any of that refuses. (server only)
	function Seat(self, vehicle: Entity, player: Player): boolean
	--- Put one of the car's liveries on it: index counts from 0 in the liveries its vehicle was published with, and -1 takes the livery off. Every player sees the change. Returns false when the entity is not a car or the car has no livery at that index (the built-in cars have none). (server only)
	function SetLivery(self, vehicle: Entity, index: number): boolean
	--- Paint the car: color is "#RRGGBB", the same format as Backpack:SetWeaponColor. Returns false when the entity is not a car. (server only)
	function SetPaintColor(self, vehicle: Entity, color: string): boolean
	--- Spawn a car at position and return it as an Entity. item is the library item id of a vehicle this place stores: the string Storage:Get returns for one of the place's storage names. Any other value errors -- an asset id, an object path, a built-in car's name, an item the place does not store, or a weapon. The car is a real replicated vehicle: every player sees it, walks up to it and can drive it. Read its Position, tag it, or Destroy it like any entity. (server only)
	function Spawn(self, item: string, position: Vector3): Entity
	--- Unlock the car, so anyone can open it again. A locked car opens only for the last player who drove it: anyone else's vehicle key rattles the door, and holding the key breaks in -- the window on that side breaks, the car unlocks, and it must be hotwired before it starts. The lock stays when the driver gets out, and its driver can also lock and unlock it from the seat (L / D-pad left). Returns false when the entity is not a car. (server only)
	function Unlock(self, vehicle: Entity): boolean
end
declare Vehicles: VehiclesService

--- Find and watch the things in the level.
declare extern type WorldService with
	--- Sweep a box of this size (its full width, depth and height, at most 100 m on any side, kept square to the world's axes) from origin along direction and return the first thing it touches, in the same table as World:Raycast, or nil. Takes the same params as World:Raycast.
	function Blockcast(self, origin: Vector3, size: Vector3, direction: Vector3, params: {MaxDistance: number?, Include: {Entity | Player}?, Exclude: {Entity | Player}?, Tag: string?, IgnorePlayers: boolean?}?): {Entity: Entity, Player: Player?, Position: Vector3, Normal: Vector3, Distance: number}?
	--- Every entity with this actor tag.
	function FindByTag(self, tag: string): {Entity}
	--- The first entity whose name (actor label) matches, or nil.
	function FindFirst(self, name: string): Entity?
	--- Every entity and player character touching a box of this size (its full width, depth and height, at most 100 m on any side, kept square to the world's axes) centred on center, nearest first. Takes the same params as World:GetEntitiesInRadius.
	function GetEntitiesInBox(self, center: Vector3, size: Vector3, params: {Include: {Entity | Player}?, Exclude: {Entity | Player}?, Tag: string?, IgnorePlayers: boolean?, MaxResults: number?}?): {Entity}
	--- Every entity and player character touching a ball of this radius (at most 50 m) around center, nearest first. Overlap-only entities (triggers) count; entities with no collision never do. A character is listed as its Entity; Players:GetPlayerFromCharacter tells whose it is. params: Include, Exclude, Tag, IgnorePlayers, and MaxResults (256 at most, and by default). Runs on this machine only; a script may make 256 spatial queries a frame.
	function GetEntitiesInRadius(self, center: Vector3, radius: number, params: {Include: {Entity | Player}?, Exclude: {Entity | Player}?, Tag: string?, IgnorePlayers: boolean?, MaxResults: number?}?): {Entity}
	--- Call handler for every entity that has the tag now, and for each one that gets it later.
	function OnTagged(self, tag: string, handler: (entity: Entity) -> ()): Connection
	--- Call handler when an entity loses the tag.
	function OnUntagged(self, tag: string, handler: (entity: Entity) -> ()): Connection
	--- Cast a ray from origin along direction and return the first thing it hits: a table of Entity, Player, Position, Normal and Distance, or nil. Player is set when the hit is a player's character. The ray is as long as direction, or params.MaxDistance when given, and at most 1 km. It passes through overlap-only entities (triggers) and never hits entities with no collision. params: MaxDistance, Include (only these entities or players count), Exclude, Tag (only entities with this tag count), IgnorePlayers. Runs on this machine only; a script may make 256 spatial queries a frame.
	function Raycast(self, origin: Vector3, direction: Vector3, params: {MaxDistance: number?, Include: {Entity | Player}?, Exclude: {Entity | Player}?, Tag: string?, IgnorePlayers: boolean?}?): {Entity: Entity, Player: Player?, Position: Vector3, Normal: Vector3, Distance: number}?
	--- Sweep a ball of this radius (at most 50 m) from origin along direction and return the first thing it touches, in the same table as World:Raycast, or nil. Distance is how far the ball's centre travelled. Takes the same params as World:Raycast.
	function Spherecast(self, origin: Vector3, radius: number, direction: Vector3, params: {MaxDistance: number?, Include: {Entity | Player}?, Exclude: {Entity | Player}?, Tag: string?, IgnorePlayers: boolean?}?): {Entity: Entity, Player: Player?, Position: Vector3, Normal: Vector3, Distance: number}?
end
declare World: WorldService

declare task: {
	wait: (seconds: number?) -> number,
	spawn: <A...>(fn: thread | (A...) -> ...any, A...) -> thread,
	defer: <A...>(fn: thread | (A...) -> ...any, A...) -> thread,
	delay: <A...>(seconds: number, fn: thread | (A...) -> ...any, A...) -> thread,
	cancel: (thread: thread) -> (),
}
declare function warn(...: any): ()
